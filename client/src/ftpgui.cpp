#include "ftpgui.h"
#include "ui_ftpgui.h"
#include <QDebug>
#include <QString>
#include <QBrush>
#include <QColor>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <istream>
#include <iostream>
#include <dirent.h>
#include <string>

FTPGUI::FTPGUI(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::FTPGUI)
    , conn_fd(-1)
    , sentence(new char[8192]())
    ,user_state(NConnect)
    ,listen_fd(-1)
    ,data_fd(-1)
    ,server_port(-1)
{
    ui->setupUi(this);
    ui->port_edit->setValidator(new QIntValidator(0, 65535));
    start_set_state();
    connect(ui->con_btn, SIGNAL(clicked()), this, SLOT(handle_connect()));
    connect(ui->login_btn, SIGNAL(clicked()), this, SLOT(handle_loginout()));
    connect(ui->ldir_btn, SIGNAL(clicked()), this, SLOT(handle_localdir()));
    connect(ui->rdir_btn, SIGNAL(clicked()), this, SLOT(handle_remotedir()));
    connect(ui->file_btn, SIGNAL(clicked()), this, SLOT(handle_file()));
    connect(ui->exit_btn, SIGNAL(clicked()), this, SLOT(handle_exit()));
    connect(ui->clear_btn, SIGNAL(clicked()), ui->cmd_list, SLOT(clear()));
    connect(ui->list_btn, SIGNAL(clicked()), ui->remote_list, SLOT(clear()));
}

FTPGUI::~FTPGUI()
{
    delete[]sentence;
    delete ui;
}

void FTPGUI::start_set_state(){
    // user pwd clear and disable
    ui->usr_edit->clear();
    ui->usr_edit->setEnabled(false);
    ui->pwd_edit->clear();
    ui->pwd_edit->setEnabled(false);
    ui->login_btn->setText(QString("Login"));
    ui->login_btn->setEnabled(false);
    // rdir ldir file clear and disable
    ui->remotedir_edit->clear();
    ui->remotedir_edit->setEnabled(false);
    ui->localdir_edit->clear();
    ui->localdir_edit->setEnabled(false);
    ui->filename2_edit->clear();
    ui->filename2_edit->setEnabled(false);
    ui->filename4_edit->clear();
    ui->filename4_edit->setEnabled(false);
    ui->remote_list->clear();
    // btn disable
    ui->rdir_btn->setEnabled(false);
    ui->ldir_btn->setEnabled(false);
    ui->file_btn->setEnabled(false);
    ui->list_btn->setEnabled(false);
}

void FTPGUI::login_set_state(){
    // rdir ldir file clear and disable
    ui->remotedir_edit->setEnabled(true);
    ui->localdir_edit->setEnabled(true);
    ui->filename2_edit->setEnabled(true);
    ui->filename4_edit->setEnabled(true);
    ui->rdir_btn->setEnabled(true);
    ui->ldir_btn->setEnabled(true);
    ui->file_btn->setEnabled(true);
    ui->list_btn->setEnabled(true);
}

void FTPGUI::write_Console(QString str, INFO_TYPE type){
    if(type == INFO_TYPE::Errorinfo)str    = "Error:  " + str;
    if(type == INFO_TYPE::Requestinfo)str  = "Request:  " + str;
    if(type == INFO_TYPE::Responseinfo)str = "Response:  " + str;
    if(type == INFO_TYPE::Normalinfo)str   = "Info:  " + str;
    ui->cmd_list->addItem(str);
    ui->cmd_list->scrollToBottom();
    QColor color = Qt::black;
    if(type == INFO_TYPE::Errorinfo)color = Qt::red;
    if(type == INFO_TYPE::Responseinfo)color = QColor(87, 195, 194);
    if(type == INFO_TYPE::Requestinfo)color = QColor(33, 119, 184);
    if(type == INFO_TYPE::Normalinfo)color = Qt::black;
    int row_number = ui->cmd_list->count() - 1;
    ui->cmd_list->item(row_number)->setForeground(color);
    return;
}

int FTPGUI::read_Response(){
    int p = 0;
    while (1) {
        int n = read(conn_fd, sentence + p, 8191 - p);
        if (n < 0) {
            QString errorinfo = QString("read response : %1(%2)").arg(strerror(errno)).arg(errno);
            return -1;
        }
        else if (n == 0) {
            break;
        }
        else {
            p += n;
            if (sentence[p - 1] == '\n') {
                break;
            }
        }
    }
    if( p >= 2 && sentence[p - 2] == '\r'){
        sentence[p - 2] = '\0';
    }
    if( p >= 1 && sentence[p -1] == '\n'){
        sentence[p - 1] = '\0';
    }
    if(strlen(sentence) > 0)write_Console(QString(sentence), INFO_TYPE::Responseinfo);
    return 0;
}

int FTPGUI::write_Request(){
    int p = 0;
        int length = strlen(sentence);
        while (p < length) {
            int n = send(conn_fd, sentence + p, length - p, MSG_NOSIGNAL);
            if (n < 0) {
                QString errorinfo = QString("write request: %1(%2)").arg(strerror(errno)).arg(errno);
                return -1;
            }
            else {
                p += n;
            }
        }
        return 0;
}

void FTPGUI::handle_connect(){
    if(ui->con_btn->text() == "Disconnect"){
        return handle_disconnect();
    }
    QString ip_qstr = ui->IP_edit->text();
    QString qport = ui->port_edit->text();
    if(ip_qstr.length() == 0 || qport.length() == 0){
        QString errorinfo = "IP or port can't be empty.";
        write_Console(errorinfo, Errorinfo);
        return;
    }
    int port = qport.toInt();
    if(port > 65535){
        QString errorinfo = "Port out of range 0~65535.";
        write_Console(errorinfo, Errorinfo);
        return;
    }
    std::string  ip_str = ip_qstr.toStdString();
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0){
        QString errorinfo = "Wrong ip address format.";
        write_Console(errorinfo, Errorinfo);
        return;
    }

    int conn_fd;
    if((conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        QString errorinfo = QString("socket(): %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, Errorinfo);
        return;
    }
    if(::connect(conn_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ){
        QString errorinfo = QString("socket(): %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, Errorinfo);
    }

    this->conn_fd = conn_fd;

    // get initial response() and write response
    if(read_Response() == -1){
        ::close(conn_fd);
        this->conn_fd = -1;
        return;
    }

    // change state
    this->user_state = USER_STATE::Nlogin;

    // disable certain widgets
    ui->IP_edit->setEnabled(false);
    ui->port_edit->setEnabled(false);
    ui->con_btn->setText("Disconnect");

    // enable certain widgets
    ui->usr_edit->setEnabled(true);
    ui->pwd_edit->setEnabled(true);
    ui->login_btn->setEnabled(true);
    return;
}

void FTPGUI::handle_disconnect(){

    strcpy(sentence, "QUIT\r\n");
    write_Request();
    read_Response();
    ::close(conn_fd);

    ui->IP_edit->clear();
    ui->IP_edit->setEnabled(true);
    ui->port_edit->clear();
    ui->port_edit->setEnabled(true);
    ui->con_btn->setText("Connect");
    user_state = USER_STATE::NConnect;

    ui->usr_edit->clear();
    ui->usr_edit->setEnabled(false);
    ui->pwd_edit->clear();
    ui->pwd_edit->setEnabled(false);
    ui->login_btn->setEnabled(false);
}

QString FTPGUI::get_remotedir(){
    int start = -1;
    int end = -1;
    int len = strlen(sentence);
    for(int i = 0;i < len; ++i){
        if(sentence[i] == '\"'){
            if(start == -1){
                start = i;
            }
            else{
                end = i;
                break;
            }
        }
    }
    if(end == -1)return QString("");
    sentence[end] = '\0';
    return QString(sentence + start + 1);
}

QString FTPGUI::get_localdir(){
    char* buffer = NULL;
    if((buffer = getcwd(NULL, 0)) == NULL){
        QString errorinfo = "Get local work directory failed";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return QString("");
    }
    else return QString(buffer);
}

void FTPGUI::handle_quit(){
    // stop any transfer
    if(user_state == USER_STATE::Transfer){
        QString text = ui->mode_cbox->currentText();
        if(text == "port")::close(listen_fd);
        ::close(data_fd);
        ui->mode_cbox->setEnabled(true);
    }
    // send quit msg
    strcpy(sentence, "QUIT");
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    write_Request();
    memset(sentence, 0, 8192);
    read_Response();
    ::close(conn_fd);
    conn_fd = -1;
    user_state = USER_STATE::NConnect;

    // reset IP and port
    ui->IP_edit->clear();
    ui->IP_edit->setEnabled(true);
    ui->port_edit->clear();
    ui->port_edit->setEnabled(true);
    ui->con_btn->setEnabled(true);

    start_set_state();
    return;
}

void FTPGUI::handle_loginout(){
    //handle qui
    if(ui->login_btn->text() == "QUIT"){
        return handle_quit();
    }
    QString qUsr_str = ui->usr_edit->text();
    QString qPwd_str = ui->pwd_edit->text();
    if(qUsr_str.length() == 0 || qPwd_str.length() == 0){
        QString errorinfo = "User name or password can't be empty.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return;
    }

    std::string Usr_str = qUsr_str.toStdString();
    std::string Pwd_str = qUsr_str.toStdString();

    // user
    sprintf(sentence, "USER %s", Usr_str.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;

    // pass
    strcpy(sentence, "PASS ******");
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    sprintf(sentence, "PASS %s\r\n", Pwd_str.c_str());
    n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;

    user_state = USER_STATE::Hlogin;

    ui->con_btn->setText("connect");
    ui->con_btn->setEnabled(false);
    ui->usr_edit->setEnabled(false);
    ui->pwd_edit->setEnabled(false);
    ui->login_btn->setText("QUIT");
    login_set_state();


    // sys
    strcpy(sentence, "SYST");
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;

    //type
    strcpy(sentence, "TYPE I");
    write_Console(QString(sentence),INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;

    // pwd
    show_remotedir();

    // set local dir
    QString local_dir = get_localdir();
    ui->localdir_edit->setText(local_dir);
    ui->localdir_edit->setCursorPosition(0);
    latest_localdir = local_dir;
}

void FTPGUI::handle_localdir(){
    if(user_state != USER_STATE::Hlogin && user_state != USER_STATE::Transfer){
        QString errorinfo = "Please connect and login first.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return;
    }

    QString selection = ui->ldir_cbox->currentText();
    if(selection == "clear"){
        ui->localdir_edit->clear();
        return;
    }
    else if(selection == "get"){
        ui->localdir_edit->setText(latest_localdir);
        ui->localdir_edit->setCursorPosition(0);
        QString normalinfo = QString("Current working dir: %1").arg(latest_localdir);
        write_Console(normalinfo, INFO_TYPE::Normalinfo);
        return;
    }
    else if(selection == "cd"){
        QString qlocal_dir = ui->localdir_edit->text();
        std::string local_dir = qlocal_dir.toStdString();
        if(qlocal_dir.length() == 0){
            QString errorinfo = "Local dir can't be empty.";
            write_Console(errorinfo, INFO_TYPE::Errorinfo);
            return;
        }
        if(opendir(local_dir.c_str()) == NULL){
            QString errorinfo = "Local dir not exist.";
            write_Console(errorinfo, INFO_TYPE::Errorinfo);
            return;
        }
        ui->localdir_edit->setCursorPosition(0);
        QString normalinfo = QString("Switch to dir: %1").arg(qlocal_dir);
        write_Console(normalinfo, INFO_TYPE::Normalinfo);
        latest_localdir = qlocal_dir;
        return;
    }
}

void FTPGUI::handle_remotedir(){
    if(user_state != USER_STATE::Hlogin && user_state != USER_STATE::Transfer){
        QString errorinfo = "Please connect and login first.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return;
    }

    QString selection = ui->rdir_cbox->currentText();
    if(selection == "clear"){
        ui->remotedir_edit->clear();
        return;
    }

    if(user_state == USER_STATE::Transfer){
        QString normalinfo = "Transfering file now, please wait";
        write_Console(normalinfo, INFO_TYPE::Normalinfo);
        return;
    }

    if(selection == "get"){
        show_remotedir();
    }
    else{
        QString qremote_dir = ui->remotedir_edit->text();
        std::string remote_dir = qremote_dir.toStdString();
        if(selection == "list"){
            user_state = USER_STATE::Transfer;
            ui->mode_cbox->setDisabled(true);
            ui->list_btn->setDisabled(true);
            handle_list(remote_dir);
            ui->mode_cbox->setEnabled(true);
            ui->list_btn->setEnabled(true);
            user_state = USER_STATE::Hlogin;
        }
        else if(qremote_dir.length() == 0){
            QString errorinfo = "Remote dir can't be empty.";
            write_Console(errorinfo, INFO_TYPE::Errorinfo);
        }
        else if(selection == "mk"){
            make_remotedir(remote_dir);
        }
        else if(selection == "del"){
            del_remotedir(remote_dir);
        }
        else if(selection == "cd"){
            change_remotedir(remote_dir);
        }
    }
}

void FTPGUI::show_remotedir(){
    strcpy(sentence, "PWD");
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;

    if(strncmp(sentence, "257", 3) != 0)return;
    QString remote_dir = get_remotedir();
    ui->remotedir_edit->setText(remote_dir);
    return;
}

void FTPGUI::make_remotedir(std::string remote_dir){
    sprintf(sentence, "MKD %s", remote_dir.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return;
    n = read_Response();
}

void FTPGUI::del_remotedir(std::string remote_dir){
    sprintf(sentence, "RMD %s", remote_dir.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return;
    n = read_Response();
}

void FTPGUI::change_remotedir(std::string remote_dir){
    sprintf(sentence, "CWD %s", remote_dir.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return;
    n = read_Response();
}

void FTPGUI::handle_file(){
    // login state
    if(user_state != USER_STATE::Hlogin && user_state != USER_STATE::Transfer){
        QString errorinfo = "Please connect and login first.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return;
    }
    // clear will not be choked
    QString selection = ui->file_cbox->currentText();
    if(selection == "clear"){
        ui->filename4_edit->clear();
        ui->filename2_edit->clear();
        return;
    }
    // other action be chocked by transfer
    if(user_state == USER_STATE::Transfer){
        QString normalinfo = "Transfering file now, please wait.";
        write_Console(normalinfo, INFO_TYPE::Normalinfo);
        return;
    }

    QString qfile4_str = ui->filename4_edit->text();
    QString qfile2_str = ui->filename2_edit->text();
    if(qfile4_str.length() == 0 || qfile2_str.length() == 0){
        QString errorinfo = "From and to arg can't be empty.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return;
    }

    std::string file4_str = qfile4_str.toStdString();
    std::string file2_str = qfile2_str.toStdString();

    if(selection == "rename"){
        rename_file(file4_str, file2_str);
        return;
    }
    else if(selection == "get"){
        ui->mode_cbox->setDisabled(true);
        user_state = USER_STATE::Transfer;
        get_file(file4_str, file2_str);
    }
    else if(selection == "put"){
        ui->mode_cbox->setDisabled(true);
        user_state = USER_STATE::Transfer;
        put_file(file4_str, file2_str);
    }
    user_state = USER_STATE::Hlogin;
    ui->mode_cbox->setEnabled(true);
    return;
}

void FTPGUI::rename_file(std::string rename4_file, std::string rename2_file){
    sprintf(sentence, "RNFR %s", rename4_file.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;
    // not exist, don't continue RNTO
    if(strncmp(sentence, "350", 3) != 0)return;

    sprintf(sentence, "RNTO %s", rename2_file.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    n = write_Request();
    if(n == -1)return;
    n = read_Response();
    if(n == -1)return;
}

// just listen at certain local port
int FTPGUI::port(){
    int lis_fd;
    struct sockaddr_in addr;
    int lis_port = rand()%(65535 - 20000 + 1) + 20000;
    if((lis_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        QString errorinfo = QString("socket() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(lis_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(lis_fd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        QString errorinfo = QString("socket() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return -1;
    }
    if (listen(lis_fd, 10) == -1) {
        QString errorinfo = QString("socket() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return -1;
    }
    int p1 = lis_port / 256;
    int p2 = lis_port % 256;

    sprintf(sentence, "PORT %d,%d,%d,%d,%d,%d", 127, 0, 0, 1, p1, p2);
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1){
        ::close(lis_fd);
        return -1;
    }
    n = read_Response();
    if(n == -1){
        ::close(lis_fd);
        return -1;
    }
    if(sentence[0] != '2'){
        ::close(lis_fd);
        return -1;
    }
    listen_fd = lis_fd;
    return 1;
}

// accept the connection and get the data fd
int FTPGUI::accept_port(){
    int dat_fd;
    if((dat_fd = accept(listen_fd, NULL, NULL)) == -1){
        QString errorinfo = QString("accept() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        ::close(listen_fd);
        return -1;
    }
    data_fd = dat_fd;
    return 1;
}

// get port from the response
int  FTPGUI::get_port(){
    int len = strlen(sentence);
    int place_one = -1;
    int place_two = -1;
    for(int i = len - 1; i >= 0; --i){
        if(sentence[i] == ','){
            if(place_two == -1){
                place_two = i;
            }
            else if(place_one == -1){
                place_one = i;
                break;
            }
        }
    }
    if(place_one == -1 || place_two == -1)return -1;
    int num_one = 0;
    int num_two = 0;
    for(int i = place_one + 1; i < len; ++i){
        int mid = sentence[i] - '0';
        if(mid < 0 || mid > 9)break;
        num_one *= 10;
        num_one += mid;
    }
    for(int i = place_two + 1; i < len; ++i){
        int mid = sentence[i] - '0';
        if(mid < 0 || mid > 9)break;
        num_two *= 10;
        num_two += mid;
    }
    return num_one * 256 + num_two;
}

// build connection for pasv mode
int FTPGUI::build_pasv_cnt(){
    QString qip = ui->IP_edit->text();
    std::string ip_str = qip.toStdString();
    struct sockaddr_in addr;
    if((data_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        QString errorinfo = QString("socket() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return -1;
    }
    // client port and ip
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    if(inet_pton(AF_INET,ip_str.c_str(), &addr.sin_addr) <= 0){
        QString errorinfo = QString("accept() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        ::close(data_fd);
        return -1;
    }
    // connect dataport to server
    if(::connect(data_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        QString errorinfo = QString("accept() %1(%2)").arg(strerror(errno)).arg(errno);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        ::close(data_fd);
        return -1;
    }
    return 1;
}

// get server listen port and try to connect
int FTPGUI::pasv(){
    strcpy(sentence, "PASV");
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1)return -1;
    n = read_Response();
    if(n == -1)return -1;
    if(sentence[0] != '2')return -1;
    server_port = get_port();
    if(server_port == -1)return -1;
    return 1;
}

int FTPGUI::writefiledata(char* buffer, int length){
    int p = 0;
    while (p < length) {
        int n = send(data_fd, buffer + p, length - p, MSG_NOSIGNAL);
        if (n < 0) {
            QString errorinfo = "Write file data failed.";
            write_Console(errorinfo, INFO_TYPE::Errorinfo);
            return -1;
        }
        else {
            p += n;
        }
    }
    return 0;
}

int FTPGUI::write2file(FILE* file){
    char buffer[8192] = "";
    memset(buffer, 0, 8192);
    int n = -1;
    while((n = read(data_fd, buffer, 8191)) > 0){
        if(fwrite(buffer, 1, n, file) != n){
            QString errorinfo = "Write data to file failed.";
            write_Console(errorinfo, INFO_TYPE::Errorinfo);
            return -1;
        }
    }
    if(n < 0){
        QString errorinfo = "Write data to file failed.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return -1;
    }
    return 0;
}

int FTPGUI::write4file(FILE* file){
    char buffer[8192] = "";
    memset(buffer, 0 , 8192);
    int c = 0;
    while((c = fread(buffer, 1, 8191, file)) > 0){
        int n = writefiledata(buffer, c);
        // write error in transferring
        if(n == -1)return -1;
    }
    if(!feof(file)){
        QString errorinfo = "Write file data failed.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return -1;
    }
    return 0;
}

void FTPGUI::put_file(std::string file4_str, std::string file2_str){
    // check if local file exists
    QString qabsolute_path;
    if(file4_str[0] == '/'){
        qabsolute_path = QString(file4_str.c_str());
    }
    else{
        qabsolute_path = latest_localdir + '/' + QString(file4_str.c_str());
    }
    std::string absolute_path = qabsolute_path.toStdString();
    if(access(absolute_path.c_str(), F_OK) != 0){
        QString errorinfo = QString("Local file doesn't exist: %1").arg(qabsolute_path);
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
        return;
    }
    // port / pasv
    QString mode = ui->mode_cbox->currentText();
    if(mode == "port" && port() == -1)return;
    else if(mode == "pasv" && pasv() == -1)return;

    // store command
    sprintf(sentence, "STOR %s", file2_str.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1){
        if(mode == "port")::close(listen_fd);
        return;
    }

    // build data fd
    if(mode == "port" && accept_port() == -1)return;
    else if(mode == "pasv" && build_pasv_cnt() == -1)return;

    // read mark
    if(read_Response() == -1 || strncmp(sentence, "150", 3) != 0){
        if(mode == "port")::close(listen_fd);
        if(mode == "pasv")::close(data_fd);
        return;
    }

    // write file
    FILE* file = fopen(absolute_path.c_str(), "rb");
    if(file != NULL){
        write4file(file);
        fclose(file);
    }
    else{
        QString errorinfo = "Can't open local file.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
    }

    //close port
    ::close(data_fd);
    if(mode == "port")::close(listen_fd);

    // read finish response
    read_Response();
}

void FTPGUI::get_file(std::string file4_str, std::string file2_str){
    // check if local file exists
    QString qabsolute_path;
    if(file2_str[0] == '/'){
        qabsolute_path = QString(file2_str.c_str());
    }
    else{
        qabsolute_path = latest_localdir + '/' + QString(file2_str.c_str());
    }
    std::string absolute_path = qabsolute_path.toStdString();

    // port / pasv
    QString mode = ui->mode_cbox->currentText();
    if(mode == "port" && port() == -1)return;
    else if(mode == "pasv" && pasv() == -1)return;

    // retr command
    sprintf(sentence, "RETR %s", file4_str.c_str());
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1){
        if(mode == "port")::close(listen_fd);
        return;
    }

    // build data fd
    if(mode == "port" && accept_port() == -1)return;
    else if(mode == "pasv" && build_pasv_cnt() == -1)return;

    // read mark
    if(read_Response() == -1 || strncmp(sentence, "150", 3) != 0){
        if(mode == "port")::close(listen_fd);
        if(mode == "pasv")::close(data_fd);
        return;
    }

    // write file
    FILE* file = fopen(absolute_path.c_str(), "wb");
    if(file != NULL){
        write2file(file);
        fclose(file);
    }
    else{
        QString errorinfo = "Can't write to local file.";
        write_Console(errorinfo, INFO_TYPE::Errorinfo);
    }

    //close port
    ::close(data_fd);
    if(mode == "port")::close(listen_fd);

    // read finish response
    read_Response();
}

void FTPGUI::handle_list(std::string remote_dir){
    ui->remote_list->clear();
    QString mode = ui->mode_cbox->currentText();
    if(mode == "port" && port() == -1)return;
    else if(mode == "pasv" && pasv() == -1)return;

    // LIST command
    if(remote_dir.length() == 0){
        strcpy(sentence, "LIST");
    }
    else{
        sprintf(sentence, "LIST %s", remote_dir.c_str());
    }
    write_Console(QString(sentence), INFO_TYPE::Requestinfo);
    strcat(sentence, "\r\n");
    int n = write_Request();
    if(n == -1){
        if(mode == "port")::close(listen_fd);
        return;
    }

    // build data fd
    if(mode == "port" && accept_port() == -1)return;
    else if(mode == "pasv" && build_pasv_cnt() == -1)return;

    // read mark
    if(read_Response() == -1 || strncmp(sentence, "150", 3) != 0){
        ::close(data_fd);
        if(mode == "port")::close(listen_fd);
        return;
    }

    QString content;
    while(true){
        memset(sentence, 0, 8192);
        n = recv(data_fd, sentence, 8191, 0);
        if(n == 0)break;
        content += QString(sentence);
    }
    QStringList content_list = content.split("\n", QString::SkipEmptyParts);
    QIcon folder_icon = QIcon(":/pic/folder.png");
    QIcon file_icon = QIcon(":/pic/file.png");
    int list_len = content_list.length();
    for(int i = 0; i < list_len; ++i){
        QListWidgetItem* item = nullptr;
        if(content_list[i][0] == 'd')item = new QListWidgetItem(folder_icon, content_list[i], ui->remote_list);
        else item = new QListWidgetItem(file_icon, content_list[i], ui->remote_list);
        ui->remote_list->addItem(item);
    }
    ::close(data_fd);
    if(mode == "port")::close(listen_fd);
    read_Response();
}

void FTPGUI::handle_exit(){
    if(user_state == USER_STATE::Hlogin || user_state == USER_STATE::Transfer){
        if(user_state == USER_STATE::Transfer){
            QString text = ui->mode_cbox->currentText();
            if(text == "port")::close(listen_fd);
            ::close(data_fd);
            ui->mode_cbox->setEnabled(true);
        }
        strcpy(sentence, "QUIT");
        write_Console(QString(sentence), INFO_TYPE::Requestinfo);
        strcat(sentence, "\r\n");
        write_Request();
        read_Response();
        ::close(conn_fd);
    }
    exit(0);
}
