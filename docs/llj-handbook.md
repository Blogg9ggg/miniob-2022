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

   

4. like 测试

  ```
  create table like_table(id int, name char);
  insert into like_table values(1, 'apple');
  insert into like_table values(11, 'pineapple');
  insert into like_table values(2, 'orange');
  insert into like_table values(6, 'grape');
  
  insert into like_table values(10, 'cherry');
  insert into like_table values(4, 'lemon');
  insert into like_table values(7, 'coconut');
  insert into like_table values(9, 'fig');
  
  select * from like_table where name like '%e';
  SELECT * FROM like_table WHERE name LIKE '%pp%';
  
  SELECT * FROM like_table WHERE name NOT LIKE '%a%';
  
  SELECT * FROM like_table WHERE name NOT LIKE 'p%' AND name NOT LIKE '%e';
  
  create table t(s1 char, s2 char, s3 char);
  insert into t values('aaaa', 'bbbbbbbbbb', 'dd');
  ```

  

5. basic 测试

  ```
  create table basic_table(id int, age int, name char);
  insert into basic_table values(1, 1, 'A');
  insert into basic_table values(2, 2, 'B');
  insert into basic_table values(3, 3, 'D');
  SELECT * FROM basic_table;
  
  create table basic_table2(id int, age int, name char);
  insert into basic_table2 values(1, 1, 'A');
  insert into basic_table2 values(2, 2, 'B');
  insert into basic_table2 values(3, 3, 'D');
  insert into basic_table2 values(4, 4, 'E');
  
  delete from basic_table2 where name='B';
  delete from basic_table2 where name='E';
  SELECT * FROM basic_table2;
  ```

  

6. 123