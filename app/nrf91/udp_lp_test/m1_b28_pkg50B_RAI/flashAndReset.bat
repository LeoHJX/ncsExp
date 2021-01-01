nrfjprog -f NRF91 --eraseall
nrfjprog -f NRF91 --program ..\uicr.hex
nrfjprog -f NRF91 --program merged.hex
nrfjprog -f NRF91 --reset
