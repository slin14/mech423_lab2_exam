# mech423_lab2_exam
## soft link to CCS workspace
```
cd <ccs_workspace>
export CCS=`pwd`
```

```
cd <repo_clon>
ln -s $CCS/ex1/main.c ex1.c
```


# Linux
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
