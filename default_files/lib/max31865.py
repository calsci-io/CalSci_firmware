from machine import Pin, SoftSPI  # type: ignore
import time

class MAX31865:
    def __init__(self, clk, miso, mosi, cs, rtd_type="3-wire", r_ref=431.0):
        self.clk = Pin(clk)
        self.mosi = Pin(mosi)
        self.miso = Pin(miso)
        self.cs = Pin(cs, Pin.OUT)
        self.cs.value(1)  # SPI inactive (CS high)
        self.rtd = SoftSPI(baudrate=100000, polarity=0, phase=1, sck=self.clk, mosi=self.mosi, miso=self.miso)
        self.r_ref = r_ref  # Reference resistor value (ohms)
        
        # Configure for 2-wire, 3-wire, or 4-wire RTD
        # 0xD0 = V_BIAS on, Auto conversion, 3-wire mode, fault detection off
        config = 0xD0 if rtd_type == "3-wire" else 0xC0  # 2-wire same as 4-wire (0xC0)
        self.cs.value(0)
        self.rtd.write(bytearray([0x80, config]))  # Write config to register 0x00 (0x80 = write flag)
        self.cs.value(1)
        time.sleep(0.1)  # Wait for stabilization

    def read_resistance(self):
        # --- Read MSB from register 0x01 ---
        self.cs.value(0)
        self.rtd.write(bytearray([0x01]))   # Set address to 0x01 (MSB)
        msb = bytearray(1)
        self.rtd.readinto(msb)
        self.cs.value(1)

        # --- Read LSB from register 0x02 ---
        self.cs.value(0)
        self.rtd.write(bytearray([0x02]))   # Set address to 0x02 (LSB)
        lsb = bytearray(1)
        self.rtd.readinto(lsb)
        self.cs.value(1)

        # Combine MSB and LSB into one 15-bit raw value
        raw = ((msb[0] << 8) | lsb[0]) >> 1  # Drop fault bit (bit 0 of LSB)

        # Convert raw value to resistance
        self.resistance_value = (raw * self.r_ref) / 32768.0
        return self.resistance_value

    def read_temperature(self):
        # Ensure resistance is updated
        self.read_resistance()
        # Linear approximation for PT100 (alpha = 0.00385)
        temperature = (self.resistance_value - 100) / (0.00385 * 100)
        return temperature

    def read_fault(self):
        self.cs.value(0)
        self.rtd.write(bytearray([0x07]))  # Fault status register address
        fault_buf = bytearray(1)
        self.rtd.readinto(fault_buf)
        self.cs.value(1)
        fault = fault_buf[0]
        return fault if fault != 0 else None  # Return fault code or None if no fault
