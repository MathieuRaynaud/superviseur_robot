#include "../header/functions.h"

char mode_start;

void write_in_queue(RT_QUEUE *, MessageToMon);

void f_server(void *arg) {
    int err;
    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    err = run_nodejs("/usr/local/bin/node", "/home/pi/Interface_Robot/server.js");

    if (err < 0) {
        printf("Failed to start nodejs: %s\n", strerror(-err));
        exit(EXIT_FAILURE);
    } else {
#ifdef _WITH_TRACE_
        printf("%s: nodejs started\n", info.name);
#endif
        open_server();
        rt_sem_broadcast(&sem_serverOk);
    }
}

void f_sendToMon(void * arg) {
    int err;
    MessageToMon msg;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef _WITH_TRACE_
    printf("%s : waiting for sem_serverOk\n", info.name);
#endif
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    while (1) {

#ifdef _WITH_TRACE_
        printf("%s : waiting for a message in queue\n", info.name);
#endif
        if (rt_queue_read(&q_messageToMon, &msg, sizeof (MessageToRobot), TM_INFINITE) >= 0) {
#ifdef _WITH_TRACE_
            printf("%s : message {%s,%s} in queue\n", info.name, msg.header, msg.data);
#endif

            send_message_to_monitor(msg.header, msg.data);
            free_msgToMon_data(&msg);
            rt_queue_free(&q_messageToMon, &msg);
        } else {
            printf("Error msg queue write: %s\n", strerror(-err));
        }
    }
}

void f_receiveFromMon(void *arg) {
    MessageFromMon msg;
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef _WITH_TRACE_
    printf("%s : waiting for sem_serverOk\n", info.name);
#endif
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    do {
#ifdef _WITH_TRACE_
        printf("%s : waiting for a message from monitor\n", info.name);
#endif
        err = receive_message_from_monitor(msg.header, msg.data);
#ifdef _WITH_TRACE_
        printf("%s: msg {header:%s,data=%s} received from UI\n", info.name, msg.header, msg.data);
#endif
        if (strcmp(msg.header, HEADER_MTS_COM_DMB) == 0) {
            if (msg.data[0] == OPEN_COM_DMB) { // Open communication supervisor-robot
#ifdef _WITH_TRACE_
                printf("%s: message open Xbee communication\n", info.name);
#endif
                rt_sem_v(&sem_openComRobot);
            }
        } else if (strcmp(msg.header, HEADER_MTS_DMB_ORDER) == 0) {
            if (msg.data[0] == DMB_START_WITHOUT_WD) { // Start robot
#ifdef _WITH_TRACE_
                printf("%s: message start robot without watchdog\n", info.name);
#endif 
                //rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                //watchdog = DMB_START_WITHOUT_WD;
                //rt_mutex_release(&mutex_watchdog);
                rt_sem_v(&sem_startRobot);
            }
            /*********** Gestion du Watchdog *************/
            /*else if (msg.data[0] == DMB_START_WITH_WD) {
#ifdef _WITH_TRACE_
                printf("%s: message start robot with watchdog\n", info.name);
#endif 
                rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                watchdog = DMB_START_WITH_WD;
                rt_mutex_release(&mutex_watchdog);
                rt_sem_v(&sem_startRobot);
            }*/
            /********************************************/
            else if ((msg.data[0] == DMB_GO_BACK)
                    || (msg.data[0] == DMB_GO_FORWARD)
                    || (msg.data[0] == DMB_GO_LEFT)
                    || (msg.data[0] == DMB_GO_RIGHT)
                    || (msg.data[0] == DMB_STOP_MOVE)) {

                rt_mutex_acquire(&mutex_move, TM_INFINITE);
                move = msg.data[0];
                rt_mutex_release(&mutex_move);
#ifdef _WITH_TRACE_
                printf("%s: message update movement with %c\n", info.name, move);
#endif

            }
        }
        /******************************************************/
        /**************** Gestion de la camera ****************/
        /******************************************************/
        else if (strcmp(msg.header, HEADER_MTS_CAMERA) == 0) {
            if (msg.data[0] == CAM_OPEN) { // Open Camera
#ifdef _WITH_TRACE_
                printf("%s: message start camera\n", info.name);
#endif 
                rt_mutex_acquire(&mutex_cameraRequest, TM_INFINITE);
                cameraRequest = 1;
                rt_mutex_release(&mutex_cameraRequest);
            } else if (msg.data[0] == CAM_CLOSE) { // Close Camera
#ifdef _WITH_TRACE_
                printf("%s: message close camera\n", info.name);
#endif 
                rt_mutex_acquire(&mutex_cameraRequest, TM_INFINITE);
                cameraRequest = 2;
                rt_mutex_release(&mutex_cameraRequest);         
            }
            else if (msg.data[0] == CAM_ASK_ARENA) { // Detect Arena
#ifdef _WITH_TRACE_
                printf("%s: message detect arena\n", info.name);
#endif 
                rt_mutex_acquire(&mutex_cameraFSMState, TM_INFINITE);
                cameraFSMState = 3;
                rt_mutex_release(&mutex_cameraFSMState);
            }
            else if (msg.data[0] == CAM_ARENA_CONFIRM) { // Detect Arena
#ifdef _WITH_TRACE_
                printf("%s: message detect arena\n", info.name);
#endif 
                rt_mutex_acquire(&mutex_arenaState, TM_INFINITE);
                arenaState = 1;
                rt_mutex_release(&mutex_arenaState);
            }
            else if (msg.data[0] == CAM_ARENA_INFIRM) { // Detect Arena
#ifdef _WITH_TRACE_
                printf("%s: message detect arena\n", info.name);
#endif 
                rt_mutex_acquire(&mutex_arenaState, TM_INFINITE);
                arenaState = 0;
                rt_mutex_release(&mutex_arenaState);
            }
        }
    } while (err > 0);

}

void f_openComRobot(void * arg) {
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    while (1) {
#ifdef _WITH_TRACE_
        printf("%s : Wait sem_openComRobot\n", info.name);
#endif
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
#ifdef _WITH_TRACE_
        printf("%s : sem_openComRobot arrived => open communication robot\n", info.name);
#endif
        err = open_communication_robot();
        if (err == 0) {
#ifdef _WITH_TRACE_
            printf("%s : the communication is opened\n", info.name);
#endif
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_ACK);
            write_in_queue(&q_messageToMon, msg);
        } else {
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
        }
    }
}

void f_startRobot(void * arg) {
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    while (1) {
#ifdef _WITH_TRACE_
        printf("%s : Wait sem_startRobot\n", info.name);
#endif
        rt_sem_p(&sem_startRobot, TM_INFINITE);
#ifdef _WITH_TRACE_
        printf("%s : sem_startRobot arrived => Start robot\n", info.name);
#endif
        rt_mutex_acquire(&mutex_com, TM_INFINITE);
        err = send_command_to_robot(DMB_START_WITHOUT_WD);
        /********* Gestion du watchdog ajoutee par nos soins **************/
        /*rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
        switch (watchdog){
            case DMB_START_WITHOUT_WD:
                err = send_command_to_robot(DMB_START_WITHOUT_WD);
                break;
            case DMB_START_WITH_WD :
                err = send_command_to_robot(DMB_START_WITH_WD);
                break;
            default : 
                printf("Unknown command for starting robot with or without watchdog");
                break;
        }
        rt_mutex_release(&mutex_watchdog);*/
        /*****************************************************************/
        rt_mutex_release(&mutex_com);
        if (err == 0) {
#ifdef _WITH_TRACE_
            printf("%s : the robot is started\n", info.name);
#endif
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_ACK);
            write_in_queue(&q_messageToMon, msg);
        } else {
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
        }
    }
}

void f_move(void *arg) {
    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    int err;

    /* PERIODIC START */
#ifdef _WITH_TRACE_
    printf("%s: start period\n", info.name);
#endif
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    while (1) {
#ifdef _WITH_TRACE_
        printf("%s: Wait period \n", info.name);
#endif
        rt_task_wait_period(NULL);
#ifdef _WITH_TRACE_
        printf("%s: Periodic activation\n", info.name);
        printf("%s: move equals %c\n", info.name, move);
#endif
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        if (robotStarted) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            rt_mutex_acquire(&mutex_com, TM_INFINITE);
            err = send_command_to_robot(move);
            printf("Retour send command to robot (move) : %d\n", err);
            rt_mutex_release(&mutex_com);
            rt_mutex_release(&mutex_move);
#ifdef _WITH_TRACE_
            printf("%s: the movement %c was sent\n", info.name, move);
#endif            
        }
        rt_mutex_release(&mutex_robotStarted);
    }
}

void write_in_queue(RT_QUEUE *queue, MessageToMon msg) {
    void *buff;
    buff = rt_queue_alloc(&q_messageToMon, sizeof (MessageToMon));
    memcpy(buff, &msg, sizeof (MessageToMon));
    rt_queue_send(&q_messageToMon, buff, sizeof (MessageToMon), Q_NORMAL);
}

/************************ A NOUS ! *************************/

void f_displayBattery(void *arg) {
    printf("   ****** Thread battery launched\n");
    rt_task_set_periodic(NULL, TM_NOW, 1000000000); // en ns
    int level;
    while (1) {
        //printf("   ****** Thread battery running\n");
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        if (robotStarted) {
            rt_mutex_acquire(&mutex_com, TM_INFINITE);
            level = send_command_to_robot(DMB_GET_VBAT);
            rt_mutex_release(&mutex_com);
            //printf("   ****** After send to robot %d\n", level);
            switch (level){
                case -1 :
                    f_errorsCounter();
                    break;
                case -2 :
                    f_errorsCounter();
                    break;
                case -3 :
                    f_errorsCounter();
                    break;
                case -4 :
                    f_errorsCounter();
                    break;
                default :
                    level += 48;
                    send_message_to_monitor(HEADER_STM_BAT, &level);
                    break;
            }
        }   
        rt_mutex_release(&mutex_robotStarted);
    }   
}

void f_errorsCounter(void) {
    printf("*****$*****Errors counter called*****$*****\n");
    rt_mutex_acquire(&mutex_errorsCounter, TM_INFINITE);
    errorsCounter++;
    if (errorsCounter >= 3){
        send_message_to_monitor(HEADER_STM_LOST_DMB, NULL);
        close_communication_robot();
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        robotStarted = 0;
        rt_mutex_release(&mutex_robotStarted);
        errorsCounter = 0;
    }
    rt_mutex_release(&mutex_errorsCounter);
}

void f_camera (void *arg){
    int err = 2;
    printf("   ****** Thread startCamera launched\n");
    
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    
    while (1){
        rt_task_wait_period(NULL);
        //printf("startCamera : Dans le while\n");
        rt_mutex_acquire(&mutex_cameraRequest, TM_INFINITE);
        //printf("startCamera : Apres prise mutex cameraRequest\n");
        if (cameraRequest == 1){
            //printf("startCamera : cameraRequest == 1\n");
            rt_mutex_acquire(&mutex_cameraFSMState, TM_INFINITE);
            //printf("startCamera : Apres prise mutex cameraFSMState\n");
            if (cameraFSMState == 0){
                err = open_camera(&RaspiCam);
                if (err == 0){
                    cameraFSMState = 1;
                    cameraRequest = 0;
                    send_message_to_monitor(HEADER_STM_ACK, "Camera started");
                } else if (err == -1){
                    send_message_to_monitor(HEADER_MTS_MSG, "Communication error with camera");
                }
            }
            rt_mutex_release(&mutex_cameraFSMState);
        }
        else if(cameraRequest == 2){
            rt_mutex_acquire(&mutex_cameraFSMState, TM_INFINITE);
            rt_mutex_acquire(&mutex_arenaState, TM_INFINITE);
            close_camera(&RaspiCam);
            send_message_to_monitor(HEADER_STM_ACK, "Camera closed");
            cameraFSMState = 0;
            cameraRequest = 0;
            arenaState = -1;
            rt_mutex_release(&mutex_arenaState);
            rt_mutex_release(&mutex_cameraFSMState);
        }
        rt_mutex_release(&mutex_cameraRequest);
    }
}

void f_envoiImages (void *arg){
    printf("   ****** Thread envoiImages launched\n");
    Image imageCapturee;
    Image imageOutput;
    Jpg  imageCompressee;
    Arene monArene;
    Position maPosition;
    
    
    rt_task_set_periodic(NULL, TM_NOW, 100000000); // en ns -> 100 ms
    
    while(1){
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_cameraFSMState, TM_INFINITE);
        if (cameraFSMState == 1){
            get_image(&RaspiCam, &imageCapturee);
            compress_image(&imageCapturee, &imageCompressee);
            send_message_to_monitor(HEADER_STM_IMAGE, &imageCompressee);
        }   else if (cameraFSMState == 2){
            rt_mutex_acquire(&mutex_arenaState, TM_INFINITE);
            if (arenaState == 1){
                get_image(&RaspiCam, &imageCapturee);
                if (detect_position(&imageCapturee, &maPosition, &monArene) < 0){
                    send_message_to_monitor(HEADER_STM_NO_ACK, "Error while detecting position");
                }
                else {
                    get_image(&RaspiCam, &imageCapturee);
                    draw_position(&imageCapturee, &imageOutput, &maPosition);
                    compress_image(&imageOutput, &imageCompressee);
                    send_message_to_monitor(HEADER_STM_IMAGE, &imageCompressee);
                }
            }
            rt_mutex_release(&mutex_arenaState);
        }
        else if (cameraFSMState == 3){
            rt_mutex_acquire(&mutex_arenaState, TM_INFINITE);
            arenaState = -1;
            rt_mutex_release(&mutex_arenaState);
            get_image(&RaspiCam, &imageCapturee);
            if(detect_arena(&imageCapturee, &monArene) < 0) {
                send_message_to_monitor(HEADER_STM_NO_ACK, "Arena not detected");
            }
            draw_arena(&imageCapturee, &imageOutput, &monArene);
            compress_image(&imageOutput, &imageCompressee);
            send_message_to_monitor(HEADER_STM_IMAGE, &imageCompressee);
            cameraFSMState = 4;
        } else if (cameraFSMState == 4){
            rt_mutex_acquire(&mutex_arenaState, TM_INFINITE);
            if (arenaState == 1){
                cameraFSMState = 3;
            } else if (arenaState == 0){
                cameraFSMState = 5;
            }
            rt_mutex_release(&mutex_arenaState);            
        } else if (cameraFSMState == 5){
            cameraFSMState = 1;          
        }
        rt_mutex_release(&mutex_cameraFSMState);
    }
}

/*void f_watchdog(void *arg){
    printf("****** Watchdog launched ******");
    while (1){
        // ON NE FAIT PAS CAR N'EST PAS IMPLEMENTE SUR LES ROBOTS
    }
}*/