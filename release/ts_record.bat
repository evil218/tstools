@set DESTDIR=f:
@set SOURCE=udp://@224.165.54.210:1231
@rem set SOURCE=udp://@:1233

@set date=%"date /t"%
@set time=%"time /t"%

@rem change ':' to '-'
@for /f "tokens=1-3* delims=.: " %%i in ("%date%_%time%") do (
    @set FILE_NAME=%DESTDIR%\%%i-%%j-%%k.ts
)

@echo transfer %SOURCE% to %FILE_NAME%
ipcat %SOURCE% | tobin %FILE_NAME%
