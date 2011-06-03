@rem set SOURCE=udp://@224.165.54.210:1234
@set SOURCE=udp://@:1234

@echo check CC of %SOURCE%
catip %SOURCE% | tsana -time -cc
