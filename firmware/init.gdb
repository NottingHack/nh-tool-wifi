target remote localhost:3333
monitor reset init

alias flash = monitor program nh-tool-wifi.elf reset
alias reset = monitor reset init
