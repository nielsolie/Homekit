python /Users/michael/Development/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/cu.SLAB_USBtoUART --baud 2000000 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x10000 /Users/michael/Development/Homekit/build/Homekit.bin 0xC95000 utils/TLV8Keystore/esp32-134248/truststore.bin 0xC9D000 truststore_empty.bin

make -j8 app flash