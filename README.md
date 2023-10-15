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
echo "<serial data stream>" | wc -w
```

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
