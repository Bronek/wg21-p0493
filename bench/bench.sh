for i in 1 2 4 8 16 24 32 40 48 56 64 ; do
CPUS=$(printf '%8s' | tr ' ' '1')
for j in $(seq 20); do 
RESULT=$(./bench -c $CPUS -s 32M 2>&1 1>/dev/null)
echo $i $RESULT
done
done
