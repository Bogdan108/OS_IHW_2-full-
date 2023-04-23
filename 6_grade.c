#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_ID 0x777 // ключ разделяемой памяти
#define PERMS 0666   // права доступа
#define DATABASE_SIZE 100

int cmp(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

void sys_err(char *msg)
{
    puts(msg);
    exit(1);
}

struct database *db;
int shmid, semid; // идентификатор разделяемой памяти
struct sembuf mybuf;
// Структура БД
struct database
{
    // Количество процессов-читателей, получивших доступ к БД
    int num_r;
    int data[DATABASE_SIZE];
};

void my_handler(int nsig)
{
    // Удаление семафора
    semctl(semid, 0, IPC_RMID, 0);

    // Удаление разделяемой памяти
    shmdt(db);
    if (shmctl(shmid, IPC_RMID, (struct shmid_ds *)0) < 0)
    {
        sys_err("server: shared memory remove error");
    }
    printf("Semaphores and Shared memory was deleted\n");
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Неправильное количество аргументов командной строки\n");
        return 1;
    }

    int number_of_writers = atoi(argv[1]);
    int number_of_readers = atoi(argv[2]);
    int mem_size = sizeof(struct database);

    key_t key;
    char pathname[] = "6_grade.c";

    key = ftok(pathname, 0);
    // Создание семафора для синхронизации процессов-писателей
    if ((semid = semget(key, 1, 0666 | IPC_CREAT)) < 0)
    {
        printf("can't create\n");
        exit(-1);
    }

    mybuf.sem_num = 0;
    mybuf.sem_op = 1;
    mybuf.sem_flg = 0;

    // устанавливаю значение 1 в качестве начального
    if (semop(semid, &mybuf, 1) < 0)
    {
        printf("can't wait\n");
        exit(-1);
    }

    // создание сегмента разделяемой памяти для БД
    if ((shmid = shmget(SHM_ID, mem_size, PERMS | IPC_CREAT)) < 0)
    {
        sys_err("server: can not create shared memory segment");
    }
    printf("Shared memory %d created\n", SHM_ID);

    // подключение сегмента к адресному пространству процесса
    if ((db = (struct database *)shmat(shmid, 0, 0)) == NULL)
    {
        sys_err("server: shared memory attach error");
    }
    printf("Shared memory pointer = %p\n", db);
    db->num_r = 0;

    // Заполнение БД начальными данными
    for (int i = 0; i < DATABASE_SIZE; i++)
    {
        db->data[i] = i;
    }

    // Создание процессов-писателей
    for (int i = 0; i < number_of_writers; i++)
    {
        if (fork() == 0)
        {
            srand(getpid());
            while (1)
            {
                // Блокировка семафора для писателей

                mybuf.sem_num = 0;
                mybuf.sem_op = -1;
                mybuf.sem_flg = 0;

                if (semop(semid, &mybuf, 1) < 0)
                {
                    printf("can't wait\n");
                    exit(-1);
                }
                // Изменение БД
                int index = rand() % DATABASE_SIZE;
                db->data[index] = rand() % 100;
                printf("Process %d wrote data to index %d\n", getpid(), index);
                qsort(&db->data, DATABASE_SIZE, sizeof(int), cmp);
                // Разблокировка семафора для писателей

                mybuf.sem_num = 0;
                mybuf.sem_op = 1;
                mybuf.sem_flg = 0;

                if (semop(semid, &mybuf, 1) < 0)
                {
                    printf("can't wait\n");
                    exit(-1);
                }
                sleep(1);
            }
            exit(0);
        }
    }

    // Создание процессов-читателей
    for (int i = 0; i < number_of_readers; i++)
    {
        if (fork() == 0)
        {
            srand(getpid());
            while (1)
            {
                (db->num_r)++;
                if (db->num_r == 1)
                {
                    // Блокировка семафора для писателей
                    mybuf.sem_num = 0;
                    mybuf.sem_op = -1;
                    mybuf.sem_flg = 0;
                    if (semop(semid, &mybuf, 1) < 0)
                    {
                        printf("can't wait\n");
                        exit(-1);
                    }
                }
                // Чтение данных из БД
                int index = rand() % DATABASE_SIZE;
                int data = db->data[index];
                printf("Process %d read data from index %d: %d\n", getpid(), index, data);
                (db->num_r)--;
                if (db->num_r == 0)
                {
                    // Блокировка семафора для писателей
                    mybuf.sem_num = 0;
                    mybuf.sem_op = 1;
                    mybuf.sem_flg = 0;

                    if (semop(semid, &mybuf, 1) < 0)
                    {
                        printf("can't wait\n");
                        exit(-1);
                    }
                }
                sleep(1);
            }
            exit(0);
        }
    }
    // обработка сигнала прерывания
    (void)signal(SIGINT, my_handler);

    // Ожидание завершения дочерних процессов
    for (int i = 0; i < number_of_readers + number_of_writers; i++)
    {
        wait(NULL);
    }
    return 0;
}