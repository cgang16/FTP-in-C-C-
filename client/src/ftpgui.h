#ifndef FTPGUI_H
#define FTPGUI_H

#include <QMainWindow>
#include <QString>

typedef enum{NConnect, Nlogin, Hlogin, Transfer}USER_STATE;
typedef enum{Errorinfo, Responseinfo, Requestinfo, Normalinfo}INFO_TYPE;

QT_BEGIN_NAMESPACE
namespace Ui { class FTPGUI; }
QT_END_NAMESPACE

class FTPGUI : public QMainWindow
{
    Q_OBJECT

public:
    FTPGUI(QWidget *parent = nullptr);
    ~FTPGUI();

public slots:

    void handle_connect();
    void handle_loginout();
    void handle_localdir();
    void handle_remotedir();
    void handle_file();
    void handle_exit();

private:

    void write_Console(QString str, INFO_TYPE type);
    int read_Response();
    int write_Request();
    QString get_remotedir();
    QString get_localdir();
    void handle_quit();
    void handle_disconnect();
    void start_set_state();
    void login_set_state();
    void show_remotedir();
    void make_remotedir(std::string remote_dir);
    void del_remotedir(std::string remote_dir);
    void change_remotedir(std::string remote_dir);
    void handle_list(std::string remote_dir);
    void rename_file(std::string rename4_file, std::string rename2_file);

    int port();
    int pasv();
    int accept_port();
    int build_pasv_cnt();
    int get_port();

    int writefiledata(char* sentence, int length);
    int write4file(FILE* file);
    int write2file(FILE* file);
    void put_file(std::string file4_str,std::string file2_str);
    void get_file(std::string file4_str,std::string file2_str);


    Ui::FTPGUI *ui;
    int conn_fd;
    char* sentence;
    USER_STATE user_state;
    QString latest_localdir;

    int listen_fd;
    int data_fd;
    int server_port;

};
#endif // FTPGUI_H
