@set PID=0x0047
@set PREFIX=f:\
@set POSTFIX=es

catts %1 | tsana -pid %PID% -es | tots %PREFIX%%PID%.%POSTFIX%
pause
