# mech423_lab2_exam
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
