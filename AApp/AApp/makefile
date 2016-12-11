hello:
	g++ aapp_service.cpp -std=c++11 -fPIC -O2 -lfastcgi-daemon2 -lcurl -shared -o libaapp.so
	g++ replication_start.cpp -std=c++11 -fPIC -O2 -lcurl -o replication
	g++ consistency_check.cpp -std=c++11 -fPIC -O2 -lcurl -o conflicts

