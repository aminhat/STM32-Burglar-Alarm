
import serial

# Connection propeties
port = 'COM15'
bound_rate = 9600

ser_con = serial.Serial(port, bound_rate)

while(1) {
    bytes data_from_port = ser_con.read_until()
}