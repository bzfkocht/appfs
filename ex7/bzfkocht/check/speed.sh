#for i in err/* nonexisting_file.dat " "
#do
#   echo ==== Testing $i =====
#   $1 $i 
#done 
for i in dat/bip_2NrNnC.dat dat/bip_6csCZ3.dat dat/bip_AYrhE1.dat dat/bip_BnF8SS.dat dat/bip_FNUjEn.dat
do
   echo Testing $1 $i 
   $1 $i 
done 
