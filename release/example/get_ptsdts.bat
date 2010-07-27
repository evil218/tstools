@set PID=0x0047

bincat %1 | tsana -pid %PID% -ptsdts
pause
