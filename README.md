# number-table
Binary encode/decode table of integer numbers.

It encodes text table of lines of `<Tab>`-separated decimal numbers into binary representation or
decodes this representation to text table. Supports delta-encoding.

## Compilation
`make.sh`

## Program usage
Program reads input data from `STDIN` and writes output data to `STDOUT`.

##### Usage:
```
    number_table -h
    number_table (-c|-d) FORMAT
```
##### Options:
```
    -h            print help and exit
    -c|-d FORMAT  encode or decode
```

`FORMAT` describes the way how to encode/decode table fields.

`FORMAT := FIELD[,FIELD,...]`  
`FIELD := (SIGN)(SIZE)[DELTA]`  
`SIGN := (s|u)` -- sign indicator: `s` - signed, `u` - unsigned  
`SIZE := (8|16|32|64)` -- size of integer number in bits  
`DELTA := d(SIGN)(SIZE)` -- use delta encoding; `SIGN` and `SIZE` specify format of delta numbers

Example `FORMAT`: `u32,s64du8`

## Example
```
$ printf "1\t-1\t256\n3\t-2\t512\n" | number_table -c 'u8,s8,u16' > nt.bin
$ xxd -g 1 -u nt.bin
00000000: 01 FF 00 01 03 FE 00 02                          ........
$ cat nt.bin | number_table -d 'u8,s8,u16'
1	-1	256
3	-2	512
```
