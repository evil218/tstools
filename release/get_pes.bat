@set PID=0x0047
@set PREFIX=f:\
@set POSTFIX=pes

tscat %1 | tsana -pid %PID% -pes | tobin %PREFIX%%PID%.%POSTFIX%
pause
