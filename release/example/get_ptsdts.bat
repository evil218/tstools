@set PID=0x0047

catbin %1 | tsana -pid %PID% -ptsdts
pause
