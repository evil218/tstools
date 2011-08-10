码流工具箱

目前有如下工具：
catts   把二进制文件（*.ts）转换成文本格式，输出到stdout
catip   把IP端口收到的码流转换成文本格式，输出到stdout
tsana   从stdin接收TS包，分析后输出到stdout
tots    从stdin接收文本格式的TS包，转换成二进制格式，输出到文件（*.ts）

组合使用这些工具，可以灵活的做TS层的各种工作：
show_pid_list.bat
show_psi_tree.bat
ts_record.bat
...

安装：
参考Makefile，
sudo make install即可把这些工具拷入linux的/usr/local/bin目录，
sudo make uninstall即可把这些工具从linux的/usr/local/bin目录中删除。

周骋
