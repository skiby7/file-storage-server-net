#!/bin/bash

stress_test_pids=()
for i in {1..15}; do
    # bash -c './test/stress_test.sh' &
    bash -c './test/stress_test.sh' &
    stress_test_pids+=($!)
    sleep 0.1
done

sleep 2400

for i in "${stress_test_pids[@]}"; do
    kill -9 ${i} &> /dev/null
    wait ${i} &> /dev/null
done


exit 0