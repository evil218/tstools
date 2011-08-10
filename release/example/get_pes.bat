@set PID=0x0047
@set PREFIX=f:\
@set POSTFIX=pes

catts %1 | tsana -pid %PID% -pes | tots %PREFIX%%PID%.%POSTFIX%
pause
