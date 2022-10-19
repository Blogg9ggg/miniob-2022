1. 在 windows 用 docker 开发

   ```
   docker run -d --name miniob --privileged -v $PWD/miniob-2022:/root/miniob -p 10086:22 oceanbase/miniob
   
   docker exec -it miniob bash
   ```

   vscode 连接：`root@localhost:10086`

   ```
   # vscode ssh 配置如下
   Host miniob-2022-docker
     HostName localhost
     User root
     Port 10086
     ForwardAgent yes
   ```

   

2. 运行数据库

   ```
   ./bin/observer -s miniob.sock -f ../etc/observer.ini &
   ./bin/obclient -s miniob.sock
   ```

   

3. 开 ssh

   ```
   /usr/sbin/sshd -f /etc/ssh/sshd_config
   ```

   

4. show index 进度
yacc_sql.y, parse.cpp, parse_defs.h