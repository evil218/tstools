@rem TS包数 = (文件长度/188)

@rem 从第START个TS包开始
@set START=451814

@rem 截取COUNT个TS包
@set COUNT=451814

catts %1 | tsana -dump -start %START% -count %COUNT% | tobin %1.ts
pause
