# mech423_lab2_exam
## Timers

| TB | P | P | TA|
|--|--|--|--|
| TB0.1 | P1.4 | *P1.0* | *TA0.1* |
| TB0.2 | P1.5 | P1.1 | TA0.2 |
| TB1.1 | P1.6 | | |
| *TB1.1* | *P3.4* | P1.2 | TA1.1 |
| TB1.2 | P1.7 | | |
| *TB1.2* | *P3.5* | P1.3 | TA1.2 |

## Circular Buffer
count number of elements in circular buffer
```
echo "<serial_data_stream>" | wc -w
```
## Ex 10 Commands
FREQ_CMD_BYTE = 1
LEDS_CMD_BYTE = 2
DUTY_CMD_BYTE = 3

 | MSG_START_BYTE | cmdByte       | data_H_Byte | data_L_Byte | escByte | data_modified |
 |----------------|---------------|-------------|-------------|---------|---------------|
 | 255            | FREQ_CMD_BYTE |             |             |         |               |
 | 255            | LEDS_CMD_BYTE |             |             |         |               |
 | 255            | DUTY_CMD_BYTE |             |             |         |               |
 | 255            |               |             | 0x00        | 0x01    | 0x..FF        |
 | 255            |               | 0x00        |             | 0x02    | 0xFF..        |
 | 255            |               | 0x00        | 0x00        | 0x03    | 0xFFFF        |

### Ex 10 Demo
default: P1.6 500Hz, 50% duty cycle (CCR0 = 2000, CCR1 = 1000)
default: LEDs all ON

 | startByte | cmdByte  | data_H_Byte | data_L_Byte | escByte | LEDs/P1.6     | explanation |
 |-----------|----------|-------------|-------------|---------|---------------|-------------|
 | 255       | 2       	| 0           |   7         | 0       | 0000 0111     |             |
 | 255       | 2       	| 0           | 170         | 0       | 1010 1010     |             |
 | 255       | 1       	| 15          | 160         | 0       | 250Hz, 25%    | CCR0 = 4000 |
 | 255       | 1       	|  7          | 207         | 0       | 500Hz, 50%    | CCR0 = 2000 |
 | 255       | 3       	|  1          | 224         | 0       | 500Hz, 25%    | CCR1 =  500 |
 | 255       | 3        |  5          | 220         | 0       | 500Hz, 75%    | CCR1 = 1500 |


## hard link to CCS workspace
```
cd <ccs_workspace>
export CCS=`pwd`
```

```
cd <repo_clone>
```

create a single hard link
```
ln $CCS/ex1/main.c ex1.c
```

create hard links for ex 1 to 10
```
for i in {1..10};
do
  ln -s $CCS/ex$i/main.c ex$i.c;
done
```

# Linux
## hard link
points a filename to data on a storage device
```
ln <file> <new_file_path>
```

## soft link
points a filename to another filename, which then points to information on a storage device
```
ln -s <file> <new_file_path>
```

## set env var
env var pointing to code composer studio workspace
```
export CCS=`pwd`
```

x5 data = frequency
cmd 1 = light to dark
cmd 2 = dark to light
cmd 4 = turn off P3.4
