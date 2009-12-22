@set PID=0x0047

tscat %1 | tsana -pid %PID% -ptsdts
pause
