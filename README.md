Зависимости: libjsoncpp, libpthread, libgmp, libjsonrpccpp-client/server/common.
libbtc, libleveldb, libsecp256k1 линкованы статически.
Пайпы для общения с демоном находятся в /tmp, в /tmp/data лежит по умолчанию база. (testpipein, testpipeout).
Демон создаст 4 лог файла в директории которой находится, для отслеживания происходящего внутри. Syslog я решил не использовать. 

Первый поток отвечает за работу с пайпами, прием отправку сообщений.
Второй поток вызывается асинхронно через каждый 60 секунд и обновляет состояние БД.
Третий, он же родительский - выполняет команды полученные из пайпа и отправляет ответы в очередь на выходной пайп.

С txindex=1 я так и не смог потестить, поскольку рескан сожрал все место на виртуалке с дебианом и не собирался останавливаться :)
Но логика реализована. 

Сборка стандартная: cd build; cmake ..; make;
