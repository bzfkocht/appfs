#for i in err/* nonexisting_file.dat " "
#do
#   echo ==== Testing $i =====
#   $1 $i 
#done 
for i in dat/*.dat err/*.dat
do
   echo ==== Testing $i =====
   $1 $i 
done 
$1 -h
$1 -V
$1 -x
$1 -v4 dat/test_scale.dat
$1 -v7
$1
$1 xxx
echo "All tests done"
