#### batch build 

mkdir ./output_hexs

mkdir ./output_hexs/nb_28
west build -b nrf9160dk_nrf9160_ns -p -- -DOVERLAY_CONFIG=nb_b28.conf
mv build/zephyr/merged.hex ./output_hexs/nb_28
cp ./flash_nrf9160_merged.hex.bat ./output_hexs/nb_28

mkdir ./output_hexs/nb_b3_8_28
west build -b nrf9160dk_nrf9160_ns -p -- -DOVERLAY_CONFIG=nb_b3_8_28.conf
mv build/zephyr/merged.hex ./output_hexs/nb_b3_8_28
cp ./flash_nrf9160_merged.hex.bat ./output_hexs/nb_b3_8_28

mkdir ./output_hexs/nb_b28_dbg_trace
west build -b nrf9160dk_nrf9160_ns -p -- -DOVERLAY_CONFIG=nb_b28_dbg_trace.conf
mv build/zephyr/merged.hex ./output_hexs/nb_b28_dbg_trace
cp ./flash_nrf9160_merged.hex.bat ./output_hexs/nb_b28_dbg_trace

mkdir ./output_hexs/nb_b3_8_28_dbg_trace
west build -b nrf9160dk_nrf9160_ns -p -- -DOVERLAY_CONFIG=nb_b3_8_28_dbg_trace.conf
mv build/zephyr/merged.hex ./output_hexs/nb_b3_8_28_dbg_trace
cp ./flash_nrf9160_merged.hex.bat ./output_hexs/nb_b3_8_28_dbg_trace

mkdir ./output_hexs/m1_b3_8_28
west build -b nrf9160dk_nrf9160_ns -p -- -DOVERLAY_CONFIG=m1_b3_8_28.conf
mv build/zephyr/merged.hex ./output_hexs/m1_b3_8_28
cp ./flash_nrf9160_merged.hex.bat ./output_hexs/m1_b3_8_28

mkdir ./output_hexs/m1_b3_8_28_dbg_trace
west build -b nrf9160dk_nrf9160_ns -p -- -DOVERLAY_CONFIG=m1_b3_8_28_dbg_trace.conf
mv build/zephyr/merged.hex ./output_hexs/m1_b3_8_28_dbg_trace
cp ./flash_nrf9160_merged.hex.bat ./output_hexs/m1_b3_8_28_dbg_trace
