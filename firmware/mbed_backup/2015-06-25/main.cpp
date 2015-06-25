#include "emmaCode.h"

//init debug port
Serial DBG(PA_9, PA_10);    //tx, rx

//init wifi port
Serial _ESP(PA_2, PA_3);    //tx, rx
//init espduino
ESP esp(&_ESP, &DBG, ESP_CH_PD, ESP_BAUD);
//init wifi mqtt
ESPMQTT mqtt(&esp);
//init wifi rest
REST rest(&esp);

//init eth port
SPI spi(PB_15, PB_14, PB_13);               //mosi, miso, sck
MQTTEthernet ipstack(&spi, PB_12, PC_6);    //spi, cs, reset

//init sd card
SDFileSystem sd(PA_7, PA_6, PA_5, PC_12, "sd"); //mosi, miso, sck, cs

//emma settings
string emmaUID;
string platformDOMAIN;
string platformKEY;
string platformSECRET;
string wifiSSID;
string wifiPASS;
string gprsAPN;
string proxySERVER;
string proxyPORT;
string proxyAUTH;

//variables
bool ethAvailable = false;
bool wifiAvailable = false;
bool gprsAvailable = false;
bool ethConnected = false;
bool wifiConnected = false;
bool useProxy = false;
bool newCommand = false;
string globalCommand;

void emmaInit(void) {
    DBG.baud(19200);
    DBG.printf("\r\nemmaInit\r\n");
    
    //read settings
    //readSetting("emmaUID");         //sd card need to be read once before working correctly
    emmaUID = readSetting("emmaUID");
    DBG.printf("emmaUID:%s\r\n",emmaUID.c_str());
    platformDOMAIN = readSetting("platformDOMAIN");
    DBG.printf("platformDOMAIN:%s\r\n",platformDOMAIN.c_str());
    platformKEY = readSetting("platformKEY");
    DBG.printf("platformKEY:%s\r\n",platformKEY.c_str());
    platformSECRET = readSetting("platformSECRET");
    DBG.printf("platformSECRET:%s\r\n",platformSECRET.c_str());
    proxySERVER = readSetting("proxySERVER");
    DBG.printf("proxySERVER:%s\r\n",proxySERVER.c_str());
    proxyPORT = readSetting("proxyPORT");
    DBG.printf("proxyPORT:%s\r\n",proxyPORT.c_str());
    proxyAUTH = readSetting("proxyAUTH");
    DBG.printf("proxyAUTH:%s\r\n",proxyAUTH.c_str());
    
    //check proxy
    if(!proxySERVER.empty() && !proxyPORT.empty() && !proxyAUTH.empty()) {
        useProxy = true;
    } else {
        useProxy = false;    
    }
    //testing purpose
    useProxy = false;
    DBG.printf("proxy:%d\r\n",useProxy);
    
    //check available interface
    availableIface();
    //testing purpose
    wifiAvailable = false;
    
    DBG.printf("wifiAvail:%d\r\n",wifiAvailable);
    DBG.printf("ethAvail:%d\r\n",ethAvailable);
}
void emmaModeWiFiConfig(void) {
    string str;
    
    if(wifiAvailable) {
        DBG.printf("emmaModeWiFiConfig\r\n");
        
        //set wifi module to configuration
        _ESP.printf("MODE=C");
        while(1) {
            char rcv[128] = {};
            rcvReply(rcv,3000);
            str = rcv;
            if(str.find("SC_STATUS_FIND_CHANNEL") != std::string::npos)
                break;
        }
        DBG.printf("entering wifi configuration mode\r\n");
        while(1) {
            char rcv[128] = {};
            rcvReply(rcv,3000);
            str = rcv;
            if(str.find("MODE=C OK") != std::string::npos) {
                //save wifiSSID and wifiPASS
                if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                    str.erase(str.begin(),str.begin()+str.find("[")+1);
                    str.erase(str.begin()+str.find("]"),str.end());
                            
                    MbedJSONValue jsonValue;
                    parse(jsonValue,str.c_str());
                        
                    char *parameter[2] = {"wifiSSID","wifiPASS"};
                        
                    for(int i=0; i<2; i++) {
                        if(jsonValue.hasMember(parameter[i])) {
                            string val = jsonValue[parameter[i]].get<std::string>();
                            int st = writeSetting(parameter[i],val.c_str());
                            if(st) {
                                DBG.printf("%s is saved\r\n",parameter[i]);
                            } else {
                                DBG.printf("%s is not saved\r\n",parameter[i]);
                            }
                        }
                    }
                } 
            }
        }    
    } else {
        DBG.printf("no wifi found\r\n");
    }
}
void emmaModeSettings(void) {
    bool clientIsConnected = false;
    bool serverIsListened = false;
    char s[32];
    string str;
    
    mkdir("/sd/settings",0777);
    //get and write emmaUID
    string uid = getUID();
    sprintf(s,"(%s)",uid.c_str());
    uid = s;
    writeSetting("emmaUID",uid);
    
    if(wifiAvailable) {
        DBG.printf("emmaModeSettings - wifi\r\n");
        
        _ESP.printf("MODE=S");
        while(1) {
            char rcv[128] = {};
            rcvReply(rcv,3000);
            str = rcv;
            if(str.find("MODE=S_OK") != std::string::npos)
                break;
        }
        DBG.printf("entering settings mode\r\n");
        while(1) {
            char rcv[512] = {};
            rcvReply(rcv,3000);
            //DBG.printf("rcv:%s\r\n",rcv);
            str = rcv;
            if(str.find("MODE=S_Config") != std::string::npos) {
                //save gprs and proxy setting
                if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                    str.erase(str.begin(),str.begin()+str.find("[")+1);
                    str.erase(str.begin()+str.find("]"),str.end());
                            
                    MbedJSONValue jsonValue;
                    parse(jsonValue,str.c_str());
                        
                    char *parameter[4] = {"gprsAPN","proxySERVER","proxyPORT","proxyAUTH"};
                        
                    for(int i=0; i<4; i++) {
                        if(jsonValue.hasMember(parameter[i])) {
                            string val = jsonValue[parameter[i]].get<std::string>();
                            int st = writeSetting(parameter[i],val.c_str());
                            if(st) {
                                DBG.printf("%s: %s is saved\r\n",parameter[i],val.c_str());
                            } else {
                                DBG.printf("%s is not saved\r\n",parameter[i]);
                            }
                        }
                    }
                }    
            } else if(str.find("connect") != std::string::npos) {
                DBG.printf("connection success!\r\n");
            }
        }
    } else if(ethAvailable) {
        DBG.printf("emmaModeSettings - eth\r\n");
        
        TCPSocketServer svr;
        TCPSocketConnection clientSock;
    
        if(svr.bind(SERVER_PORT) < 0) {
            DBG.printf("tcp server bind failed\r\n");    
        } else {
            DBG.printf("tcp server bind success\r\n");
            serverIsListened = true;    
        }
    
        DBG.printf("please connect to %s\r\n",ipstack.getEth().getIPAddress());
        
        if(svr.listen(1) < 0) {
            DBG.printf("tcp server listen failed\r\n");    
        } else {
            DBG.printf("tcp server is listening...\r\n");    
        }
    
        clientSock.set_blocking(false,30000);   //timeout after 30sec
    
        //listening
        while (serverIsListened) {
            if(svr.accept(clientSock) < 0) {
                DBG.printf("failed to accept connection\r\n");    
            } else {
                DBG.printf("connection success!\r\nIP: %s\r\n",clientSock.get_address());
                clientIsConnected = true;
            
                while(clientIsConnected) {
                    char buffer[1024] = {};
                    switch(clientSock.receive(buffer,1023)) {
                        case 0:
                            DBG.printf("received buffer is empty\r\n");
                            clientIsConnected = false;
                            break;
                        case -1:
                            DBG.printf("failed to read data from client\r\n");
                            clientIsConnected = false;
                            break;
                        default:
                            //DBG.printf("received data: %d\r\n%s\r\n",strlen(buffer),buffer);
                            DBG.printf("\r\n");
                        
                            str = buffer;
                            if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                                str.erase(str.begin(),str.begin()+str.find("[")+1);
                                str.erase(str.begin()+str.find("]"),str.end());
                            
                                MbedJSONValue jsonValue;
                                parse(jsonValue,str.c_str());
                        
                                char *parameter[4] = {"gprsAPN","proxySERVER","proxyPORT","proxyAUTH"};
                        
                                for(int i=0; i<4; i++) {
                                    if(jsonValue.hasMember(parameter[i])) {
                                        string val = jsonValue[parameter[i]].get<std::string>();
                                        int st = writeSetting(parameter[i],val.c_str());
                                        if(st) {
                                            DBG.printf("%s: %s is saved\r\n",parameter[i],val.c_str());
                                        } else {
                                            DBG.printf("%s is not saved\r\n",parameter[i]);
                                        }
                                    }
                                }
                            }
                            break;     
                    }    
                }
                DBG.printf("close connection\r\n");
                clientSock.close();
            }
        }
    } else {
        DBG.printf("no wifi or eth found\r\n");
    }
}
void emmaModeRegister(void) {
    bool emmaGetRegKey = false;
    bool emmaRegistered = false;
    char s[512];
    char r[256];
    int connPort;
    int loop = 0;
    string connData;
    string connHost;
    string hmac;
    string str;
    string regKey;
    Timer t;
    
    //calculate hmac
    for(int i=0; i<sizeof(s); i++) {
        s[i]=0; }
    sprintf(s,"emma-%s",emmaUID.c_str());   
    hmac = calculateMD5(s);
    DBG.printf("hmac:%s\r\n",hmac.c_str());
    
    if(wifiAvailable) {
        DBG.printf("emmaModeRegister - wifi\r\n");
        
        _ESP.printf("MODE=B");
        wait(1);
        while(!esp.ready());
        
        //set connHost, connPort
        if(useProxy) {
            connHost = proxySERVER;
            sscanf(proxyPORT.c_str(),"%d",&connPort);
        } else {
            connHost = EMMA_SERVER_HOST;
            connPort = EMMA_SERVER_PORT;
        }
        
        //rest begin
        if(!rest.begin(connHost.c_str(),connPort,false)) {
            DBG.printf("EMMA: fail to setup rest");
            while(1);    
        }
        wifiConnected = true;   //with custom firmware, wifi module should connect automatically
        
        esp.process();
        if(wifiConnected) {
            //set connData
            if(useProxy) {
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                sprintf(s,"http://%s:%d/emma/api/controller/register?uid=%s&hmac=%s",EMMA_SERVER_HOST,EMMA_SERVER_PORT,emmaUID.c_str(),hmac.c_str());
                connData = s;
            } else {
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                sprintf(s,"/emma/api/controller/register?uid=%s&hmac=%s",emmaUID.c_str(),hmac.c_str());
                connData = s;
            }
         
            //register
            while(!emmaGetRegKey) {
                rest.get(connData.c_str());
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                rest.getResponse(s,sizeof(s));
                DBG.printf("rsp reg:%s\r\n",s);

                //check and save platform setting
                str = s;
                if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                    str.erase(str.begin(),str.begin()+str.find("[")+1);
                    str.erase(str.begin()+str.find("]"),str.end());
        
                    MbedJSONValue jsonValue;
                    parse(jsonValue,str.c_str());
    
                    char *parameter[4] = {"platformDOMAIN","platformKEY","platformSECRET","registrationKey"};
    
                    //save platform parameter
                    writeSetting(parameter[0],"()");    //sd card need to be initialized
                    for(int i=0; i<3; i++) {
                        if(jsonValue.hasMember(parameter[i])) {
                            string val = jsonValue[parameter[i]].get<std::string>();
                            int st = writeSetting(parameter[i],val.c_str());
                            if(st) {
                                DBG.printf("%s: %s is saved\r\n",parameter[i],val.c_str());
                            } else {
                                DBG.printf("%s is not saved\r\n",parameter[i]);
                            }
                        }
                    }
        
                    //get registrationKey
                    if(jsonValue.hasMember(parameter[3])) {
                        string val = jsonValue[parameter[3]].get<std::string>();
                        if(val.find("(") != std::string::npos && val.find(")") != std::string::npos) {
                            val.erase(val.begin(),val.begin()+val.find("(")+1);
                            val.erase(val.begin()+val.find(")"),val.end());
                            regKey = val;
                            DBG.printf("%s: %s\r\n",parameter[3],regKey.c_str());
                            emmaGetRegKey = true;
                        }
                    }
                }
            }
                
            //calculate hmac
            for(int i=0; i<sizeof(r); i++) {
                r[i]=0; }
            sprintf(r,"emma-%s-%s",emmaUID.c_str(),regKey.c_str());
            hmac = calculateMD5(r);
            DBG.printf("hmac:%s\r\n",hmac.c_str());
            
            //set connData
            if(useProxy) {
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                sprintf(s,"http://%s:%d/emma/api/controller/verify?uid=%s&registrationKey=%s&hmac=%s",EMMA_SERVER_HOST,EMMA_SERVER_PORT,emmaUID.c_str(),regKey.c_str(),hmac.c_str());
                connData = s;
            } else {
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                sprintf(s,"/emma/api/controller/verify?uid=%s&registrationKey=%s&hmac=%s",emmaUID.c_str(),regKey.c_str(),hmac.c_str());
                connData = s;
            }
                
            //verify registration
            while(!emmaRegistered && loop < 12){
                rest.get(connData.c_str());
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                rest.getResponse(s,sizeof(s));
                DBG.printf("rsp vrf:%s\r\n",s);
                
                //check verification
                str = s;
                if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                    str.erase(str.begin(),str.begin()+str.find("[")+1);
                    str.erase(str.begin()+str.find("]"),str.end());
                    
                    MbedJSONValue jsonValue;
                    parse(jsonValue,str.c_str());
                    if(jsonValue.hasMember("user")) {
                        string val = jsonValue["user"].get<std::string>();
                        DBG.printf("%s is registered\r\n",val.c_str());
                        emmaRegistered = true;
                    }
                }
                wait(5);
                loop++;
            }
            
            //check whether registration success
            if(emmaRegistered) {
                DBG.printf("registration successful\r\n");
            } else {
                DBG.printf("registration unsuccessful\r\n");
            }
            while(1);
        }
        
    } else if(ethAvailable) {
        DBG.printf("emmaModeRegister - eth\r\n");
        
        //set connHost, connPort, connData
        if(useProxy) {
            DBG.printf("use proxy\r\n");
            connHost = proxySERVER;
            sscanf(proxyPORT.c_str(),"%d",&connPort);
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET http://%s:%d/emma/api/controller/register?uid=%s&hmac=%s HTTP/1.0\nHost: %s\r\n\r\n",EMMA_SERVER_HOST,EMMA_SERVER_PORT,emmaUID.c_str(),hmac.c_str(),EMMA_SERVER_HOST);
            connData = s;
        } else {
            DBG.printf("no proxy\r\n");
            connHost = EMMA_SERVER_HOST;
            connPort = EMMA_SERVER_PORT;
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET /emma/api/controller/register?uid=%s&hmac=%s HTTP/1.0\nHost: %s\r\n\r\n",emmaUID.c_str(),hmac.c_str(),EMMA_SERVER_HOST);
            connData = s;
        }
        
        //register
        while(!emmaGetRegKey) {
            str = "";
            str = ethGET(connHost,connPort,connData);
            DBG.printf("rsp reg:%s\r\n",str.c_str());

            //check and save platform setting
            if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                str.erase(str.begin(),str.begin()+str.find("[")+1);
                str.erase(str.begin()+str.find("]"),str.end());
        
                MbedJSONValue jsonValue;
                parse(jsonValue,str.c_str());
    
                char *parameter[4] = {"platformDOMAIN","platformKEY","platformSECRET","registrationKey"};
    
                //save platform parameter
                writeSetting(parameter[0],"()");    //sd card need to be initialized
                for(int i=0; i<3; i++) {
                    if(jsonValue.hasMember(parameter[i])) {
                        string val = jsonValue[parameter[i]].get<std::string>();
                        int st = writeSetting(parameter[i],val.c_str());
                        if(st) {
                            DBG.printf("%s: %s is saved\r\n",parameter[i],val.c_str());
                        } else {
                            DBG.printf("%s is not saved\r\n",parameter[i]);
                        }
                    }
                }
        
                //get registrationKey
                if(jsonValue.hasMember(parameter[3])) {
                    string val = jsonValue[parameter[3]].get<std::string>();
                    if(val.find("(") != std::string::npos && val.find(")") != std::string::npos) {
                        val.erase(val.begin(),val.begin()+val.find("(")+1);
                        val.erase(val.begin()+val.find(")"),val.end());
                        regKey = val;
                        DBG.printf("%s: %s\r\n",parameter[3],regKey.c_str());
                        emmaGetRegKey = true;
                    }
                }
            }
        }
        
        //calculate hmac
        for(int i=0; i<sizeof(r); i++) {
            r[i]=0; }
        sprintf(r,"emma-%s-%s",emmaUID.c_str(),regKey.c_str());
        hmac = calculateMD5(r);
        DBG.printf("hmac:%s\r\n",hmac.c_str());
        
        //set connData
        if(useProxy) {
            DBG.printf("use proxy\r\n");
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET http://%s:%d/emma/api/controller/verify?uid=%s&registrationKey=%s&hmac=%s HTTP/1.0\nHost: %s\r\n\r\n",EMMA_SERVER_HOST,EMMA_SERVER_PORT,emmaUID.c_str(),regKey.c_str(),hmac.c_str(),EMMA_SERVER_HOST);
            connData = s;
        } else {
            DBG.printf("no proxy\r\n");
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET /emma/api/controller/verify?uid=%s&registrationKey=%s&hmac=%s HTTP/1.0\nHost: %s\r\n\r\n",emmaUID.c_str(),regKey.c_str(),hmac.c_str(),EMMA_SERVER_HOST);
            connData = s;
        }
        
        //verify registration
        while(!emmaRegistered && loop < 12){
            str = "";
            str = ethGET(connHost,connPort,connData);
            DBG.printf("rsp vrf:%s\r\n",str.c_str());
                
            //check verification
            if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                str.erase(str.begin(),str.begin()+str.find("[")+1);
                str.erase(str.begin()+str.find("]"),str.end());
                    
                MbedJSONValue jsonValue;
                parse(jsonValue,str.c_str());
                if(jsonValue.hasMember("user")) {
                    string val = jsonValue["user"].get<std::string>();
                    DBG.printf("%s is registered\r\n",val.c_str());
                    emmaRegistered = true;
                }
            }
            wait(5);
            loop++;
        }
            
        //check whether registration success
        if(emmaRegistered) {
            DBG.printf("registration successful\r\n");
        } else {
            DBG.printf("registration unsuccessful\r\n");
        }
        while(1);
        
    } else if(gprsAvailable) {
        DBG.printf("emmaModeRegister - gprs\r\n");
        
    } else {
        DBG.printf("no wifi, eth, or gprs found\r\n");
    }
    
    
/*    
    if(ethAvailable) {
        DBG.printf("emmaModeRegister - eth\r\n");
        
        //calculate hmac
        sprintf(s,"emma-%s",emmaUID.c_str());   
        hmac = calculateMD5(s);
        DBG.printf("hmac:%s\r\n",hmac.c_str());
        
        //register
        if(useProxy) {
            DBG.printf("use proxy\r\n");
            connHost = proxySERVER;
            sscanf(proxyPORT.c_str(),"%d",&connPort);
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET http://%s:%d/emma/api/controller/register?uid=%s&hmac=%s HTTP/1.0\nHost: %s\r\n\r\n",EMMA_SERVER_HOST,EMMA_SERVER_PORT,emmaUID.c_str(),hmac.c_str(),EMMA_SERVER_HOST);
        } else {
            DBG.printf("no proxy\r\n");
            connHost = EMMA_SERVER_HOST;
            connPort = EMMA_SERVER_PORT;
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET /emma/api/controller/register?uid=%s&hmac=%s HTTP/1.0\nHost: %s\r\n\r\n",emmaUID.c_str(),hmac.c_str(),EMMA_SERVER_HOST);
        }
        str = ethGET(connHost,connPort,s);
        DBG.printf("response:%s\r\n",str.c_str());
        
        //check and save platform setting
        if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
            str.erase(str.begin(),str.begin()+str.find("[")+1);
            str.erase(str.begin()+str.find("]"),str.end());
        
            MbedJSONValue jsonValue;
            parse(jsonValue,str.c_str());
    
            char *parameter[4] = {"platformDOMAIN","platformKEY","platformSECRET","registrationKey"};
    
            //save platform parameter
            writeSetting(parameter[0],"()");    //sd card need to be written once before working correctly
            for(int i=0; i<3; i++) {
                if(jsonValue.hasMember(parameter[i])) {
                    string val = jsonValue[parameter[i]].get<std::string>();
                    int st = writeSetting(parameter[i],val.c_str());
                    if(st) {
                        DBG.printf("%s: %s is saved\r\n",parameter[i],val.c_str());
                    } else {
                        DBG.printf("%s is not saved\r\n",parameter[i]);
                    }
                }
            }
        
            //get registrationKey
            if(jsonValue.hasMember(parameter[3])) {
                string val = jsonValue[parameter[3]].get<std::string>();
                if(val.find("(") != std::string::npos && val.find(")") != std::string::npos) {
                    val.erase(val.begin(),val.begin()+val.find("(")+1);
                    val.erase(val.begin()+val.find(")"),val.end());
                    regKey = val;
                    DBG.printf("%s: %s\r\n",parameter[3],regKey.c_str());
                    emmaGetRegKey = true;
                }
            }
        }
            
        //calculate hmac
        //for(int i=0; i<sizeof(r); i++) {
        //    r[i]=0; }
        //sprintf(r,"emma-%s-%s",emmaUID.c_str(),regKey.c_str());
        //hmac = calculateMD5(r);
        //DBG.printf("hmac:%s\r\n",hmac.c_str());
        
    } else if(wifiAvailable) {
        DBG.printf("emmaModeRegister - wifi\r\n");
    } else if(gprsAvailable) {
        DBG.printf("emmaModeRegister - gprs\r\n");    
    } else {
        DBG.printf("no eth, wifi, or gprs found\r\n");
    }
*/
    
    //DBG.printf("emmaModeRegister\r\n");
    
    //wifi interface
    //DBG.printf("wifi interface\r\n");
    
    //bool emmaGetRegKey = false;
    //bool emmaRegistered = false;
    //char r[256];
    //char s[1024];
    //int loop = 0;
    //string hmac;
    //string regKey;
    //string str;
/*    
    //calculate hmac
    for(int i=0; i<sizeof(s); i++) {
        s[i]=0; }
    sprintf(s,"emma-%s",emmaUID.c_str());   
    hmac = calculateMD5(s);
    //hmac = "575fc5328804e40510f9b615c41e422a";
    DBG.printf("hmac:%s\r\n",hmac.c_str());
    
    //wait until receiving MODE: INIT in custom esp firmware
    Timer t;
    t.start();
    while(1) {
        char rcv[128] = {};
        rcvReply(rcv,3000);
        str = rcv;
        if(str.find("MODE: INIT") != std::string::npos || t.read_ms() > 6000) {
            t.stop();
            break;
        }
    }
    
    DBG.printf("wifi should be ready\r\n");
*/       

/*    
    _ESP.printf("MODE=B");
    DBG.printf("MODE=B\r\n");
    
    esp.enable();
    wait(1);
    while(!esp.ready());
    DBG.printf("EMMA: setup rest client\r\n");
    
    //start with widha's service
    //if(!rest.begin("192.168.131.200",8080,false)) {
    if(!rest.begin(EMMA_SERVER_HOST,EMMA_SERVER_PORT,false)) {
        DBG.printf("EMMA: fail to setup rest");
        while(1);    
    }
    
    DBG.printf("EMMA: setup wifi\r\n");
    
    //without smart config
    //esp.wifiCb.attach(&restWifiCb); 
    //esp.wifiConnect("Belkin_G_Plus_MIMO","");
    
    //with smart config
    wifiConnected = true;
    
    DBG.printf("EMMA: system started\r\n");
    
    while(1) {
        esp.process();
        if(wifiConnected) {
            //register
            while(!emmaGetRegKey) {
                for(int i=0; i<sizeof(r); i++) {
                    r[i]=0; }
                sprintf(r,"/emma/api/controller/register?uid=%s&hmac=%s",emmaUID.c_str(),hmac.c_str());
                rest.get(r);
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                rest.getResponse(s,sizeof(s));
                DBG.printf("rsp reg:%s\r\n",s);

                //check and save platform setting
                str = s;
                if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                    str.erase(str.begin(),str.begin()+str.find("[")+1);
                    str.erase(str.begin()+str.find("]"),str.end());
        
                    MbedJSONValue jsonValue;
                    parse(jsonValue,str.c_str());
    
                    char *parameter[4] = {"platformDOMAIN","platformKEY","platformSECRET","registrationKey"};
    
                    //save platform parameter
                    writeSetting(parameter[0],"()");    //sd card need to be initialized
                    for(int i=0; i<3; i++) {
                        if(jsonValue.hasMember(parameter[i])) {
                            string val = jsonValue[parameter[i]].get<std::string>();
                            int j = writeSetting(parameter[i],val.c_str());
                            if(j) {
                                DBG.printf("%s is saved\r\n",parameter[i]);
                            } else {
                                DBG.printf("%s is not saved\r\n",parameter[i]);
                            }
                        }
                    }
        
                    //get registrationKey
                    if(jsonValue.hasMember(parameter[3])) {
                        string val = jsonValue[parameter[3]].get<std::string>();
                        if(val.find("(") != std::string::npos && val.find(")") != std::string::npos) {
                            val.erase(val.begin(),val.begin()+val.find("(")+1);
                            val.erase(val.begin()+val.find(")"),val.end());
                            regKey = val;
                            DBG.printf("%s is %s\r\n",parameter[3],regKey.c_str());
                            emmaGetRegKey = true;
                        }
                    }
                }
            
                //calculate hmac
                for(int i=0; i<sizeof(r); i++) {
                    r[i]=0; }
                sprintf(r,"emma-%s-%s",emmaUID.c_str(),regKey.c_str());
                hmac = calculateMD5(r);
                DBG.printf("hmac:%s\r\n",hmac.c_str());
            }
            
            //verify registration
            while(!emmaRegistered && loop < 12){
                for(int i=0; i<sizeof(r); i++) {
                    r[i]=0; }
                sprintf(r,"/emma/api/controller/verify?uid=%s&registrationKey=%s&hmac=%s",emmaUID.c_str(),regKey.c_str(),hmac.c_str());
                //DBG.printf("cmd vrf:%s\r\n",r);
                rest.get(r);
                for(int i=0; i<sizeof(s); i++) {
                    s[i]=0; }
                rest.getResponse(s,sizeof(s));
                DBG.printf("rsp vrf:%s\r\n",s);
                
                //check verification
                str = s;
                if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
                    str.erase(str.begin(),str.begin()+str.find("[")+1);
                    str.erase(str.begin()+str.find("]"),str.end());
                    
                    MbedJSONValue jsonValue;
                    parse(jsonValue,str.c_str());
                    if(jsonValue.hasMember("user")) {
                        string val = jsonValue["user"].get<std::string>();
                        DBG.printf("%s is registered\r\n",val.c_str());
                        emmaRegistered = true;
                    }
                }
                wait(5);
                loop++;
            }
            
            //check whether registration success
            if(emmaRegistered) {
                DBG.printf("registration successful\r\n");
            } else {
                DBG.printf("registration unsuccessful\r\n");
            }
            while(1);
        }
    }
*/


/*
    //not finished yet        
    //eth interface
    DBG.printf("eth interface\r\n");
    
    //variables
    char s[512];
    int n;
    string hmac;
    string str;
    string regKey;
    
    //get emmaUID
    //emmaUID = getUID();
    //DBG.printf("emmaUID:%s\r\n",emmaUID.c_str());
    //emmaUID = "0674ff575349896767072538";
    
    //calculate hmac
    sprintf(s,"emma-%s",emmaUID.c_str());   
    hmac = calculateMD5(s);
    DBG.printf("hmac:%s\r\n",hmac.c_str());
    //hmac = "575fc5328804e40510f9b615c41e422a";
    
    //read proxy setting
    //proxySERVER = readSetting("proxySERVER");
    //DBG.printf("proxySERVER:%s\r\n",proxySERVER.c_str());
    //proxyPORT = readSetting("proxyPORT");
    //DBG.printf("proxyPORT:%s\r\n",proxyPORT.c_str());
    //proxyAUTH = readSetting("proxyAUTH");
    //DBG.printf("proxyAUTH:%s\r\n",proxyAUTH.c_str());
    
    //check if use proxy
    if(!proxySERVER.empty() && !proxyPORT.empty() && !proxyAUTH.empty()) {
        useProxy = true;
    } else {
        useProxy = false;    
    }
    
    //hardcoded for development
    useProxy = false;
      
    //register
    if(useProxy) {
        DBG.printf("use proxy\r\n");
        sprintf(s,"GET https://api.thingspeak.com/talkbacks/887/commands/last?api_key=ACM8XW24UDVY1GTV HTTP/1.0\nHost: api.thingspeak.com\nProxy-Authorization: Basic %s\r\n\r\n",proxyAUTH.c_str());
        sscanf(proxyPORT.c_str(),"%d",&n);
        str = ethGET(proxySERVER,n,s);
        DBG.printf("response:%s\r\n",str.c_str());
    } else {
        DBG.printf("no proxy\r\n");
        //memset(s,0,sizeof(s));
        for(int i=0; i<sizeof(s); i++) {
            s[i]=0; }
        sprintf(s,"GET /emma/emmacontrollerapi/register?uid=%s&hmac=%s HTTP/1.0\nHost: 192.168.131.200\r\n\r\n",emmaUID.c_str(),hmac.c_str());
        //sprintf(s,"GET /emma/data/register HTTP/1.0\r\n\r\n");  //to local server
        DBG.printf("command:%s\r\n",s);
        //str = ethGET("192.168.2.43",9000,s);
        str = ethGET("192.168.131.200",8080,s);
        //str = ethGET("192.168.128.69",80,s);
        DBG.printf("response:%s\r\n",str.c_str());    
    }
    while(1);
    
    //check and save platform setting
    if(str.find("[") != std::string::npos && str.find("]") != std::string::npos) {
        str.erase(str.begin(),str.begin()+str.find("[")+1);
        str.erase(str.begin()+str.find("]"),str.end());
        
        MbedJSONValue jsonValue;
        parse(jsonValue,str.c_str());
    
        char *parameter[4] = {"platformDOMAIN","platformKEY","platformSECRET","registrationKey"};
    
        //save platform parameter
        for(int i=0; i<3; i++) {
            if(jsonValue.hasMember(parameter[i])) {
                string val = jsonValue[parameter[i]].get<std::string>();
                int j = writeSetting(parameter[i],val.c_str());
                DBG.printf("%s is %d\r\n",parameter[i],j);
            }
        }
        
        //get registrationKey
        if(jsonValue.hasMember(parameter[3])) {
            string val = jsonValue[parameter[3]].get<std::string>();
            if(val.find("(") != std::string::npos && val.find(")") != std::string::npos) {
                val.erase(val.begin(),val.begin()+val.find("(")+1);
                val.erase(val.begin()+val.find(")"),val.end());
                regKey = val;
                DBG.printf("%s is %s\r\n",parameter[3],regKey.c_str());
            }
        }
    }
    
    //calculate hmac
    sprintf(s,"emma-%s-%s",emmaUID.c_str(),regKey.c_str());   
    hmac = calculateMD5(s);
    //DBG.printf("hmac:%s\r\n",hmac.c_str());
    
    //verify registration
    if(useProxy) {
        DBG.printf("verify - proxy\r\n");
        sprintf(s,"GET https://api.thingspeak.com/talkbacks/887/commands/last?api_key=ACM8XW24UDVY1GTV HTTP/1.0\nHost: api.thingspeak.com\nProxy-Authorization: Basic %s\r\n\r\n",proxyAUTH.c_str());
        sscanf(proxyPORT.c_str(),"%d",&n);
        str = ethGET(proxySERVER,n,s);
        DBG.printf("response:%s\r\n",str.c_str());
    } else {
        DBG.printf("verify - direct\r\n");
        sprintf(s,"GET /emmacontrollerapi/verify?uid=%s&registrationKey=%s&hmac=%s HTTP/1.0\r\n\r\n",emmaUID.c_str(),regKey.c_str(),hmac.c_str());
        str = ethGET("192.168.2.43",9000,s);
        DBG.printf("response:%s\r\n",str.c_str());
        DBG.printf("finish response\r\n");   
    }
    //--------------------------------------------------------------------------------------------------------------------------------------------
*/
/*
    TCPSocketConnection sock;
    for(int a=0; a<5; a++) {
        //registration
        //sock.connect("192.168.1.101",9000);      //to widha's pc
        sock.connect("169.254.135.111",9000);      //to widha's pc (direct connection)
        //sock.connect("192.168.128.69",80);      //to local server
        //sock.connect("192.168.128.5",3128);   //use proxy
        //sock.connect("184.106.153.149", 80);   //api.thingspeak.com
    
        DBG.printf("socket-connect\r\n");
    
        sprintf(s,"GET /emmacontrollerapi/register?uid=%s&hmac=%s HTTP/1.0\r\n\r\n",emmaUID.c_str(),hmac.c_str());  //to widha's pc
        //sprintf(s,"GET /emma/data/register HTTP/1.0\r\n\r\n");  //to local server
        //sprintf(s,"GET http://192.168.2.43:9000/emmacontrollerapi/register?uid=%s&hmac=%s HTTP/1.0\nHost: 192.168.2.43\nProxy-Authorization: Basic Og==\r\n\r\n",uid,hmac);  //use proxy
        //sprintf(s,"GET https://api.thingspeak.com/talkbacks/887/commands/last?api_key=ACM8XW24UDVY1GTV HTTP/1.0\nHost: api.thingspeak.com\nProxy-Authorization: Basic %s\r\n\r\n",proxyAUTH.c_str());
        //sprintf(s,"GET /talkbacks/887/commands/last?api_key=ACM8XW24UDVY1GTV HTTP/1.0\n\n");
    
        DBG.printf("command:%s\r\n",s);
    
        sock.send_all(s,sizeof(s)-1);
        DBG.printf("socket-send\r\n");
        wait(2);
    
        //receive return
        char buf[1024];
        int ret;
        Timer t;
        
        t.start();
        while(1) {
            ret = sock.receive(buf, sizeof(buf));
            if(ret<=0 || t.read_ms() > 20000)
                t.stop();
                break;    
        }
        DBG.printf("return:%s\r\n",buf);
        sock.close();
        wait(5);
    }
    DBG.printf("end of loop\r\n");
*/
}

void emmaModeOperation(void) {
    DBG.printf("emmaModeOperation\r\n");
    
    //wait until wifi ready
    char s[1024];
    string str;
    Timer ti;
    
    ti.start();
    while(1) {
        char rcv[128] = {};
        rcvReply(rcv,3000);
        str = rcv;
        if(str.find("MODE: INIT") != std::string::npos || ti.read_ms() > 6000) {
            ti.stop();
            break;
        }
    }
    
    //set wifi to mode bridge
    _ESP.printf("MODE=B");
    while(1) {
        char rcv[128] = {};
        rcvReply(rcv,3000);
        str = rcv;
        if(str.find("MODE=B_OK") != std::string::npos)
            break;
    }
    DBG.printf("MODE=B\r\n");
    
    int i=0;
    Timer t;
    char mqttClientId[32];
    char mqttTopic[64];
    char mqttPayload[64];
    
    esp.enable();
    wait(1);
    while(!esp.ready());
    
    DBG.printf("emma: setup mqtt client\r\n");
    sprintf(mqttClientId,"emma/%s",emmaUID.c_str());
    
    //if(!mqtt.begin("emma/066eff575349896767073038", "761b233e-a49a-4830-a8ae-87cec3dc1086", "ef25cf4567fbc07113252f8d72b7faf2", 120, 1)) {
    if(!mqtt.begin(mqttClientId, platformKEY.c_str(), platformSECRET.c_str(), 120, 1)) {
        DBG.printf("emma: fail to setup mqtt");
        while(1);    
    }
    
    DBG.printf("emma: setup mqtt lwt\r\n");
    mqtt.lwt("/lwt", "offline", 0, 0);
    
    mqtt.connectedCb.attach(&mqttConnected);
    mqtt.disconnectedCb.attach(&mqttDisconnected);
    mqtt.publishedCb.attach(&mqttPublished);
    mqtt.dataCb.attach(&mqttData);
    
    DBG.printf("emma: setup wifi\r\n");
    
    //without smart config
    //esp.wifiCb.attach(&wifiCb);
    //esp.wifiConnect("Belkin_G_Plus_MIMO","");
    
    //with smart config
    mqtt.connect(MQTT_HOST,MQTT_PORT,false);
    wifiConnected = true;
    
    DBG.printf("emma: system started\r\n");
    
    t.start();
    while(1) {
        esp.process();
        if(wifiConnected) {
            //publish data
            if(t.read_ms() > 10000) {
                sprintf(mqttTopic,"%s/%s/panelTemp",platformDOMAIN.c_str(),emmaUID.c_str());
                sprintf(mqttPayload,"{\"time\":\"20150615-12:32:12\",\"value\":%d}",i);
                DBG.printf(mqttPayload);
                DBG.printf("\r\n");
                mqtt.publish(mqttTopic,mqttPayload);
                /*mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelTemp",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelHum",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelGas",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelCh1",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelCh2",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelCh3",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/nodeTemp",mqttPayload);
                mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/cmdExecution","success");
                */
                i++;
                t.reset();    
            }
            
            //check for new command
            if(newCommand) {
                DBG.printf("newCommand\r\n");
                MbedJSONValue jsonValue;
                parse(jsonValue,globalCommand.c_str());
                char *parameter[5] = {"id","nType","nAddr","dType","cmd"};
                
                //check if command is valid
                bool validCommand = true;
                for(int i=0; i<5; i++) {
                    validCommand = validCommand && jsonValue.hasMember(parameter[i]);
                }
                DBG.printf("is command valid:%d\r\n",validCommand);
                
                if(validCommand) {
                    string commandId = jsonValue[parameter[0]].get<std::string>();
                    string commandNType = jsonValue[parameter[1]].get<std::string>();
                    string commandNAddr = jsonValue[parameter[2]].get<std::string>();
                    string commandDType = jsonValue[parameter[3]].get<std::string>();
                    string commandCmd = jsonValue[parameter[4]].get<std::string>();
                    
                    if(commandNType == "0") {       //switch on panel controller
                        DBG.printf("command for switch\r\n");
                    }
                    else if(commandNType == "1") {  //node with mac address
                        DBG.printf("command for node\r\n");
                        //get node ip address based on node mac address
                        string nodeIP = readNodeIP(commandNAddr);
                        DBG.printf("nodeIP: %s\r\n",nodeIP.c_str());
                        
                        //get cmd string based on device type and command number
                        string nodeCmd = readNodeCmd(commandDType,commandCmd);
                        DBG.printf("nodeCmd: %s\r\n",nodeCmd.c_str());
                        
                        //execute command
                        if(rest.begin(nodeIP.c_str(),REMOTE_TCP_PORT,false)) {
                            sprintf(s,"<?xml version=\"1.0\" encoding=\"utf-8\"?><app_cmd cmd=\"5\" /><app_data code=\"%s\"/>\r\n",nodeCmd.c_str());
                            
                            int trial=0;
                            string execStatus = "failed";
                            while(1) {
                                char rcv[1024] = {};
                                rest.get("/",s);
                                rcvReply(rcv,2000);
                                str = rcv;
                                if(str.find("REST: Sent") != std::string::npos) {
                                    DBG.printf("cmd executed\r\n");
                                    execStatus = "success";
                                    break;    
                                }
                                if(trial>0) {   //two times trial
                                    DBG.printf("cmd is not executed\r\n");
                                    break;    
                                }
                                trial++;
                                wait(3);
                            }
                            
                            //send execution status
                            sprintf(mqttTopic,"%s/%s/executionResult",platformDOMAIN.c_str(),emmaUID.c_str());
                            sprintf(mqttPayload,"{\"id\":\"%s\",\"nType\":\"%s\",\"nAddr\":\"%s\",\"dType\":\"%s\",\"cmd\":\"%s\",\"result\":\"%s\"}",
                            commandId.c_str(),commandNType.c_str(),commandNAddr.c_str(),commandDType.c_str(),commandCmd.c_str(),execStatus.c_str());
                            DBG.printf(mqttPayload);
                            DBG.printf("\r\n");
                            mqtt.publish(mqttTopic,mqttPayload);
                        }
                    }
                }
                newCommand = false;
            }   
        }    
    }    
}

/*start wifi mqtt*/
void wifiCb(void* response) {
    uint32_t status;
    RESPONSE res(response);
    
    if(res.getArgc() == 1) {
        res.popArgs((uint8_t*)&status,4);
        if(status == STATION_GOT_IP) {
            DBG.printf("WIFI Connected\r\n");
            mqtt.connect(MQTT_HOST,MQTT_PORT,false);
            wifiConnected = true;
        }
        else {
            wifiConnected = false;
            mqtt.disconnect();    
        }
    }
}
void mqttConnected(void* response) {
    DBG.printf("Connected\r\n");
    char mqttTopic[64];
    sprintf(mqttTopic,"%s/%s/command",platformDOMAIN.c_str(),emmaUID.c_str());
    mqtt.subscribe(mqttTopic);
}
void mqttDisconnected(void* response) {
    DBG.printf("Disconnected\r\n");    
}
void mqttData(void* response) {
    RESPONSE res(response);
    
    //DBG.printf("Received:\r\n");
    //DBG.printf("topic=");
    string topic = res.popString();
    DBG.printf(topic.c_str());
    DBG.printf("\r\n");
    
    //DBG.printf("command=");
    string cmd = res.popString();
    DBG.printf(cmd.c_str());
    DBG.printf("\r\n");
    
    //check whether cmd is json
    if(cmd.find("[") != std::string::npos && cmd.find("]") != std::string::npos) {
        cmd.erase(cmd.begin(),cmd.begin()+cmd.find("[")+1);
        cmd.erase(cmd.begin()+cmd.find("]"),cmd.end());
        globalCommand = cmd;
        newCommand = true;
    }
}
void mqttPublished(void* response) {    
}
/*end wifi mqtt*/

/*start wifi rest*/
void restWifiCb(void* response) {
    uint32_t status;
    RESPONSE res(response);
    
    if(res.getArgc() == 1) {
        res.popArgs((uint8_t*)&status,4);
        if(status == STATION_GOT_IP) {
            DBG.printf("WIFI Connected\r\n");
            wifiConnected = true;
        }
        else {
            wifiConnected = false;    
        }
    }
}
/*end wifi rest*/

/*start emma settings*/
string getUID(void) {
    char s[32];
    unsigned long *unique = (unsigned long *)BASE_ADDR;
    sprintf(s,"%08x%08x%08x",unique[0], unique[1], unique[2]);
    return s;    
}

string readSetting(string parameter) {
    FILE *fp;
    signed char c;
    int i=0;
    char s[64];
    string strS;
    
    sprintf(s,"/sd/settings/%s.txt",parameter.c_str());
    
    fp = fopen(s,"r");
    memset(s,0,sizeof(s));
    if(fp != NULL) {
        while(1) {
            c = fgetc(fp);
            if(c == EOF){
                break;
            }
            s[i] = c;
            i++;
        }
        strS = s;
        if(strS.find("(") != std::string::npos && strS.find(")") != std::string::npos) {
            strS.erase(strS.begin(),strS.begin()+strS.find("(")+1);
            strS.erase(strS.begin()+strS.find(")"),strS.end());
        } else {
            strS = "";    
        }
    }
    fclose(fp);
    return strS;    
}

bool writeSetting(string parameter, string value) {
    FILE *fp;
    char s[255];
    
    sprintf(s,"/sd/settings/%s.txt",parameter.c_str());
    fp = fopen(s,"w");
    if(fp != NULL) {
        fprintf(fp,value.c_str());
        fclose(fp);
        return true;
    }
    return false;
}
/*end emma settings*/

/*start emma node*/
string readNodeIP(string macAddr) {
    FILE *fp;
    signed char c;
    int i=0;
    char s[64];
    string strS;
    
    sprintf(s,"/sd/nodeList/%s/nodeIP.txt",macAddr.c_str());
    
    fp = fopen(s,"r");
    memset(s,0,sizeof(s));
    if(fp != NULL) {
        while(1) {
            c = fgetc(fp);
            if(c == EOF){
                break;
            }
            s[i] = c;
            i++;
        }
        strS = s;
        if(strS.find("(") != std::string::npos && strS.find(")") != std::string::npos) {
            strS.erase(strS.begin(),strS.begin()+strS.find("(")+1);
            strS.erase(strS.begin()+strS.find(")"),strS.end());
        } else {
            strS = "";    
        }
    }
    fclose(fp);
    return strS;
}

string readNodeCmd(string dType, string cmd) {
    FILE *fp;
    signed char c;
    int i=0;
    char s[128];
    string strS;
    
    sprintf(s,"/sd/nodeCode/%s/%s.txt",dType.c_str(),cmd.c_str());
    
    fp = fopen(s,"r");
    memset(s,0,sizeof(s));
    if(fp != NULL) {
        while(1) {
            c = fgetc(fp);
            if(c == EOF){
                break;
            }
            s[i] = c;
            i++;
        }
        strS = s;
        if(strS.find("(") != std::string::npos && strS.find(")") != std::string::npos) {
            strS.erase(strS.begin(),strS.begin()+strS.find("(")+1);
            strS.erase(strS.begin()+strS.find(")"),strS.end());
        } else {
            strS = "";    
        }
    }
    fclose(fp);
    return strS;    
}
/*end emma node*/

/*start emma connection function*/
string ethGET(string host, int port, string url) {
    char buf[1024];
    char s[256];
    int ret;
    TCPSocketConnection sock;
    Timer t;
    
    sprintf(s,"%s",url.c_str());
    sock.connect(host.c_str(),port);
    sock.send_all(s,sizeof(s)-1);
    wait(2);
    
    //receive return
    t.start();
    while(1) {
        ret = sock.receive(buf, sizeof(buf));
        if(ret<=0 || t.read_ms() > 10000) {
            t.stop();
            break;
        }    
    }
    sock.close();
    return buf;
}
/*end emma connection function*/

/*start emma private function*/
void availableIface(void) {
    //wifi interface
    string str;
    Timer t;
    
    //should create mode to check if there is wifi interface
/*
    _ESP.printf("MODE=B");
    t.start();
    while(1) {
        char rcv[64] = {};
        rcvReply(rcv,1000);
        str = rcv;
        if(str.find("MODE=B_OK") != std::string::npos) {
            t.stop();
            wifiAvailable = true;
            esp.reset();
            wait(5);
            break;
        }
        if(t.read_ms() > 2000) {
            t.stop();
            wifiAvailable = false;
            break;
        }
    }
*/
    
    //eth interface
    if(ipstack.getEth().linkstatus()){
        str = ipstack.getEth().getIPAddress();
        if(str.find("255.255.255.255") != std::string::npos) {
            ethAvailable = false;    
        } else {
            ethAvailable = true;    
        }
    } else {
        ethAvailable = false;
    }
    
    //gprs interface    
}

void connectedIface(void) { //WARNING: should be run in emmaModeRegister and emmaModeOperation
    char s[512];
    int connPort;
    string connHost;
    string str;
    Timer t;
    
    //start not working
    //wifi interface
    if(wifiAvailable) {
        _ESP.printf("MODE=B");
        if(useProxy) {
            connHost = proxySERVER;
            sscanf(proxyPORT.c_str(),"%d",&connPort);
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"http://%s:%d/emma/api/web/test HTTP/1.0\nHost: %s\r\n\r\n",EMMA_SERVER_HOST,EMMA_SERVER_PORT,EMMA_SERVER_HOST);
        } else {
            connHost = EMMA_SERVER_HOST;
            connPort = EMMA_SERVER_PORT;
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"/emma/api/web/test");
        }
        while(!esp.ready());
        if(rest.begin(connHost.c_str(),connPort,false)) {
            DBG.printf("rest begin\r\n");
            esp.process();
            rest.get(s);
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            rest.getResponse(s,sizeof(s));
            str = s;
            DBG.printf("response:%s\r\n",s);
            if(str.find("OK") != std::string::npos) {
                wifiConnected = true;
            }
        } else {
            wifiConnected = false;    
        }
    } else {
        wifiConnected = false;    
    }
    //end not working
    
    //eth interface
    if(ethAvailable) {
        if(useProxy) {
            connHost = proxySERVER;
            sscanf(proxyPORT.c_str(),"%d",&connPort);
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            sprintf(s,"GET http://%s:%d/emma/api/web/test HTTP/1.0\nHost: %s\r\n\r\n",EMMA_SERVER_HOST,EMMA_SERVER_PORT,EMMA_SERVER_HOST);
        } else {
            connHost = EMMA_SERVER_HOST;
            connPort = EMMA_SERVER_PORT;
            for(int i=0; i<sizeof(s); i++) {
                s[i]=0; }
            strcpy(s,"GET /emma/api/web/test HTTP/1.0\nHost: %s\r\n\r\n");
        }
        
        t.start();
        while(1) {
            str = ethGET(connHost,connPort,s);
            if(str.find("OK") != std::string::npos) {
                t.stop();
                ethConnected = true;
                break;    
            }
            if(t.read_ms() > 5000) {
                t.stop();
                ethConnected = false;
                break;    
            }
        }
    } else {
        ethConnected = false;    
    }
    
    //gprs interface    
}

void addChar(char *s, char c) {
    uint16_t k;     //customized for EMS
    k = strlen(s);
    s[k] = c;
    s[k + 1] = 0;
}

void rcvReply(char *r, int to) {
    Timer t;
    bool ended = false;
    char c;
    
    strcpy(r,"");
    t.start();
    while(!ended) {
        if(_ESP.readable()) {
            c = _ESP.getc();
            addChar(r,c);
            t.start();
        }
        if(t.read_ms() > to) {
            ended = true;    
        }    
    }
    addChar(r, 0x00);    
}

string calculateMD5(string text) {
    char s[64];
    memset(s,0,sizeof(s));  //for unknown reason, after reading UID, the 's' will contaion UID data
    uint8_t hash[16];
    MD5::computeHash(hash, (uint8_t*)text.c_str(), strlen(text.c_str()));
    for(int i=0; i<16; ++i) {
        sprintf(s,"%s%02x",s,hash[i]);
    }
    return s;
}
/*end emma private function*/