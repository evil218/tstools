@set PID=0x0047
@set PREFIX=f:\
@set POSTFIX=es

catbin %1 | tsana -pid %PID% -es | tobin %PREFIX%%PID%.%POSTFIX%
pause
