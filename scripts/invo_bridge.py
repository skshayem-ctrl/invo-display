#!/usr/bin/env python3
"""
invo_bridge.py — Megmeet inverter RS485 → /tmp/invo_data
Reads Modbus RTU registers every 3s, writes key=value pairs for invo_uart.c
"""
import struct, time, os, sys
import serial

PORT     = '/dev/inverter' if os.path.exists('/dev/inverter') else '/dev/ttyUSB1'
BAUD     = 9600
SLAVE    = 1
OUT_FILE = '/tmp/invo_data'
CMD_FILE = '/home/intelli/invo_cmd'
INTERVAL = 3   # seconds between polls

# Battery capacity in Ah — adjust to match your battery bank
BATT_CAPACITY_AH = 100

# EMA smoothing factor (0.1 = very smooth, 0.5 = responsive)
BATT_EMA_ALPHA = 0.15
TEMP_EMA_ALPHA = 0.2

_smooth_batt_pct    = None   # running EMA state
_smooth_temp        = None
_smooth_grid_v      = None   # EMA for grid voltage
_last_good          = {}     # last successfully written data
_batt_valid_streak  = 0      # consecutive valid battery voltage readings
BATT_VALID_MIN      = 3      # require this many in a row before trusting SOC

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc

def read_regs(port, slave, start, count):
    frame = struct.pack(">BBHH", slave, 0x03, start, count)
    frame += struct.pack("<H", crc16(frame))
    port.reset_input_buffer()
    port.write(frame)
    expected = 5 + count * 2
    time.sleep(0.08)
    resp = port.read(expected)
    if len(resp) < expected:
        time.sleep(0.08)
        resp += port.read(expected - len(resp))
    if len(resp) < expected:
        return None
    if resp[1] & 0x80:
        return None
    bc = resp[2]
    if bc != count * 2:     # byte count must match what we asked for
        return None
    return [struct.unpack(">H", resp[3+i:5+i])[0] for i in range(0, bc, 2)]

def write_reg(port, slave, addr, value):
    frame = struct.pack(">BBHH", slave, 0x06, addr, value & 0xFFFF)
    frame += struct.pack("<H", crc16(frame))
    port.reset_input_buffer()
    port.write(frame)
    time.sleep(0.2)
    resp = port.read(8)
    return resp

def check_cmd(port):
    if not os.path.exists(CMD_FILE):
        return False
    try:
        with open(CMD_FILE) as f:
            cmd = f.read().strip()
        os.remove(CMD_FILE)
        if cmd == 'output_on':
            write_reg(port, SLAVE, 4049, 1)
            print('[invo_bridge] CMD: output ON', flush=True)
            time.sleep(4)
            return True
        elif cmd == 'output_off':
            write_reg(port, SLAVE, 4049, 0)
            print('[invo_bridge] CMD: output OFF', flush=True)
            time.sleep(4)
            return True
    except Exception as e:
        print(f'[invo_bridge] CMD error: {e}', flush=True)
    return False

def s16(v):
    return v if v < 0x8000 else v - 0x10000

def batt_pct_from_voltage(volts, rated_v=24):
    """Linear SOC estimate from terminal voltage."""
    if rated_v == 48:
        lo, hi = 44.0, 57.6
    elif rated_v == 12:
        lo, hi = 11.0, 13.8
    else:  # 24V default
        lo, hi = 22.0, 28.8
    pct = (volts - lo) / (hi - lo) * 100.0
    return max(0.0, min(100.0, pct))

def write_data(data):
    tmp = OUT_FILE + '.tmp'
    with open(tmp, 'w') as f:
        for k, v in data.items():
            f.write(f"{k}={v}\n")
    os.replace(tmp, OUT_FILE)

def poll(port):
    # OVP=60V / UVP=39V confirms this is a 48V battery system
    batt_rated_v = 48

    # Two reads instead of one 27-reg burst — inverters often cap PDU at ~18-20 regs
    r1 = read_regs(port, SLAVE, 4017, 17)   # 4017-4033
    if r1 is None or len(r1) < 17:
        return None
    time.sleep(0.05)
    r2 = read_regs(port, SLAVE, 4036, 8)    # 4036-4043
    if r2 is None or len(r2) < 8:
        return None

    pv_v        = r1[0]  * 0.1        # 4017 PV voltage
    pv_a        = s16(r1[1])  * 0.1   # 4018 PV current
    pv_w        = s16(r1[2])           # 4019 PV power W
    batt_v      = r1[7]  * 0.1        # 4024 battery voltage
    batt_a      = s16(r1[8])  * 0.1   # 4025 battery current
    batt_w      = s16(r1[9])           # 4026 battery charge power W
    raw_grid_v  = r1[11] * 0.1        # 4028 grid voltage
    grid_a      = s16(r1[12]) * 0.1   # 4029 grid current
    grid_w      = s16(r1[13])          # 4030 grid input power W
    grid_chg_w  = s16(r1[14])          # 4031 grid charge power W
    grid_hz     = r1[15] * 0.01       # 4032 grid frequency
    grid_status = r1[16]               # 4033 grid status (0=abnormal 1=normal)
    inv_out_v   = r2[0]  * 0.1        # 4036 inverter stage output voltage (0 in bypass)
    out_a       = s16(r2[1]) * 0.1    # 4037 output current
    out_w       = s16(r2[2])           # 4038 AC output power W
    out_hz      = r2[3]  * 0.01       # 4039 AC output frequency
    op_status   = r2[4]                # 4040 operating status
    relay_st    = r2[5]                # 4041 relay status
    inv_t       = s16(r2[7]) * 0.1    # 4043 inverter temperature
    is_bypassing  = bool(op_status & (1 << 10))
    inv_on        = bool(op_status & (1 <<  8))
    ac_charging   = bool(op_status & (1 <<  9))
    fault_active  = bool(op_status & (1 << 11))
    op_mode       = op_status & 0xFF
    # In bypass mode the inverter stage is off (4036=0); grid passes straight through
    out_v = raw_grid_v if is_bypassing else inv_out_v

    # Discard frames with physically impossible AC frequency — catches noise frames
    # that passed the byte-count check by coincidence
    if 0 < grid_hz < 44.0 or grid_hz > 56.0:
        return None
    if 0 < out_hz < 44.0 or out_hz > 56.0:
        return None
    if not (-10.0 <= inv_t <= 120.0):
        return None

    global _smooth_batt_pct, _smooth_temp, _batt_valid_streak

    # Battery must have BOTH a plausible voltage AND measurable current to be trusted.
    # A floating ADC reads ~0A even when voltage looks valid — this rules out phantom readings.
    batt_lo = batt_rated_v * 0.60
    batt_hi = batt_rated_v * 1.20
    batt_valid = (batt_lo <= batt_v <= batt_hi) and (abs(batt_a) >= 0.5)

    if batt_valid:
        _batt_valid_streak += 1
    else:
        _batt_valid_streak = 0
        _smooth_batt_pct = 0.0   # hard reset — no battery means 0%, immediately

    # Only update SOC after BATT_VALID_MIN consecutive valid readings
    if batt_valid and _batt_valid_streak >= BATT_VALID_MIN:
        raw_pct = batt_pct_from_voltage(batt_v, batt_rated_v)
        if _smooth_batt_pct is None or _smooth_batt_pct == 0.0:
            _smooth_batt_pct = raw_pct
        elif abs(raw_pct - _smooth_batt_pct) < 25:
            _smooth_batt_pct = BATT_EMA_ALPHA * raw_pct + (1 - BATT_EMA_ALPHA) * _smooth_batt_pct

    if _smooth_batt_pct is None:
        _smooth_batt_pct = 0.0

    pct = round(_smooth_batt_pct, 1)

    # Temperature — reject readings outside sane range or jumping >3°C per 3s cycle
    if -10.0 <= inv_t <= 100.0:
        if _smooth_temp is None:
            _smooth_temp = inv_t
        elif abs(inv_t - _smooth_temp) <= 3.0:
            _smooth_temp = TEMP_EMA_ALPHA * inv_t + (1 - TEMP_EMA_ALPHA) * _smooth_temp
        # else: silently discard the spike, hold current value

    if _smooth_temp is None:
        _smooth_temp = 0.0

    temp = round(_smooth_temp, 1)

    # Grid voltage — only trust if >80V (real grid), hold last good on timeout/zero
    global _smooth_grid_v
    if raw_grid_v >= 80.0:
        if _smooth_grid_v is None or _smooth_grid_v < 80.0:
            _smooth_grid_v = raw_grid_v          # snap directly — avoids EMA blocking 0→224V jump
        elif abs(raw_grid_v - _smooth_grid_v) <= 20.0:
            _smooth_grid_v = 0.1 * raw_grid_v + 0.9 * _smooth_grid_v
    if _smooth_grid_v is None:
        _smooth_grid_v = 0.0

    grid_v = round(_smooth_grid_v, 1)

    # Battery voltage — only report when battery is confirmed connected
    reported_batt_v = round(batt_v, 1) if batt_valid else 0.0

    # Only calculate backup time when there's a real load (>10W)
    if out_w > 10 and pct > 0:
        remaining_wh = (pct / 100.0) * BATT_CAPACITY_AH * batt_rated_v
        backup_min = min((remaining_wh / out_w) * 60.0, 9999.0)
    else:
        backup_min = 0.0

    return {
        'solar_kw':    round(max(0.0, pv_w)  / 1000.0, 3),
        'solar_v':     round(max(0.0, pv_v),  1),
        'solar_a':     round(max(0.0, pv_a),  1),
        'load_kw':     round(max(0.0, out_w)  / 1000.0, 3),
        'load_peak':   round(max(0.0, out_w)  / 1000.0, 3),
        'batt_pct':    pct,
        'batt_v':      reported_batt_v,
        'batt_a':      round(batt_a, 2),
        'batt_w':      batt_w,
        'batt_chg_kw': round(batt_w / 1000.0, 3),
        'batt_temp':   temp,
        'batt_backup': round(backup_min, 0),
        'grid_v':      grid_v,
        'grid_hz':     round(grid_hz, 2),
        'grid_w':      grid_w,
        'grid_a':      round(grid_a, 2),
        'grid_chg_w':  grid_chg_w,
        'grid_status': grid_status,
        'out_v':       round(out_v, 1),
        'out_hz':      round(out_hz, 2),
        'out_a':       round(out_a, 2),
        'out_w':       out_w,
        'inv_out_v':   round(inv_out_v, 1),
        'op_status':   op_status,
        'op_mode':     op_mode,
        'bypassing':   int(is_bypassing),
        'inv_on':      int(inv_on),
        'ac_chg':      int(ac_charging),
        'fault':       int(fault_active),
        'relay_st':    relay_st,
    }

def main():
    print(f"[invo_bridge] Starting — {PORT} @ {BAUD} baud → {OUT_FILE}", flush=True)
    # Only create cmd file if absent — don't overwrite a pending command from LVGL
    if not os.path.exists(CMD_FILE):
        open(CMD_FILE, 'w').close()

    while True:
        try:
            with serial.Serial(PORT, BAUD, bytesize=8, parity='N',
                               stopbits=1, timeout=2) as port:
                print(f"[invo_bridge] Connected", flush=True)
                time.sleep(0.5)
                consecutive_errors = 0

                while True:
                    t0 = time.time()
                    try:
                        if check_cmd(port):
                            consecutive_errors = 0
                        data = poll(port)
                        if data:
                            _last_good.update(data)
                            write_data(data)
                            consecutive_errors = 0
                            print(
                                f"[invo_bridge] "
                                f"grid={data['grid_v']}V/{data['grid_hz']}Hz/{data['grid_w']}W  "
                                f"pv={data['solar_v']}V/{data['solar_a']}A/{data['solar_kw']}kW  "
                                f"batt={data['batt_pct']}%/{data['batt_v']}V/{data['batt_chg_kw']}kW  "
                                f"out={data['out_v']}V/{data['out_hz']}Hz  "
                                f"temp={data['batt_temp']}C  "
                                f"op=0x{data['op_status']:04X}(mode={data['op_mode']} byp={data['bypassing']} inv={data['inv_on']} acchg={data['ac_chg']} flt={data['fault']})  "
                                f"relay=0x{data['relay_st']:02X}  "
                                f"reg4036={data['inv_out_v']}V",
                                flush=True
                            )
                        else:
                            consecutive_errors += 1
                            print(f"[invo_bridge] No response ({consecutive_errors})", flush=True)
                            if _last_good:
                                write_data(_last_good)
                            if consecutive_errors >= 30:
                                # Inverter quiet period — flush buffers, keep port open
                                port.reset_input_buffer()
                                port.reset_output_buffer()
                                consecutive_errors = 0
                                print("[invo_bridge] Buffer reset, waiting for inverter", flush=True)
                    except serial.SerialException:
                        raise   # propagate to outer handler to reopen port
                    except Exception as e:
                        print(f"[invo_bridge] Poll error: {e}", flush=True)

                    elapsed = time.time() - t0
                    time.sleep(max(0, INTERVAL - elapsed))

        except serial.SerialException as e:
            print(f"[invo_bridge] Serial error: {e} — retry in 5s", flush=True)
            time.sleep(5)
        except Exception as e:
            print(f"[invo_bridge] Unexpected error: {e} — retry in 5s", flush=True)
            time.sleep(5)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n[invo_bridge] Stopped.", flush=True)
