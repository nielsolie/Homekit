#!/bin/sh

deviceIds=("esp32-CAFEEC" "esp32-AF5FA4" "esp32-0C9D6C" "esp32-CB3DC4" "esp32-134248")
containerId=6 

for i in "${deviceIds[@]}"
do

	mkdir -p "$i"
	python3 makeKeystore.py "$i" -c "${containerId}"

	python3 $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate truststore.csv "$i"/truststore.bin 0x8000 --version 2

	echo "Signing truststore ..."
	pk_sign ../../certs/rootCA/Sign/SigningPrivateKey.pem data.tlv8.pre
	echo "OK"

	echo "Verifying signature ..."
	pk_verify ../../certs/rootCA/Sign/SigningPublicKey.pem data.tlv8.pre
	echo "OK"


	echo "Creating truststore.tlv8 ..."
	cat data.tlv8.pre > "$i"/truststore.tlv8

	size="$(wc -c data.tlv8.pre.sig | awk '{print $1}')"
	hex=$( printf "%x" $size ) 

	#echo -n -e '\xFE\x46' | hexf 

	output="\xFE\x$hex"
	printf "%b" $output >> "$i"/truststore.tlv8

	cat data.tlv8.pre.sig >> "$i"/truststore.tlv8

	echo "OK"

	echo "Cleaning ..."
	rm data.tlv8.pre.sig
	rm data.tlv8.pre
	rm truststore.csv
	echo "OK"

done