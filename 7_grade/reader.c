#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#define DATABASE_SIZE 100

int cmp(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

struct database *db;
char memn[] = "shared-memory";
int shm;

// Структура БД
struct database
{
    // Количество процессов-читателей, получивших доступ к БД
    int num_r;
    int data[DATABASE_SIZE];
};

void my_handler(int nsig)
{
    // Закрытие разделяемой памяти
    close(shm);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Неправильное количество аргументов командной строки\n");
        return 1;
    }

    int mem_size = sizeof(struct database);
    int number_of_readers = atoi(argv[1]);

    // Создание именованного семафора для синхронизации процессов-писателей
    sem_t *write_semaphore = sem_open("write_semaphore", O_CREAT, 0666, 1);

    if ((shm = shm_open(memn, O_CREAT | O_RDWR, 0666)) == -1)
    {
        printf("Object is already open\n");
        perror("shm_open");
        return 1;
    }
    else
    {
        printf("Object is open: name = %s, id = 0x%x\n", memn, shm);
    }

    // отображаем разделяемую память в адресное пространство процесса
    db = mmap(0, mem_size, PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);

    if (db == MAP_FAILED)
    {
        printf("Error getting pointer to shared memory\n");
        perror("mmap");
        return 1;
    }
    db->num_r = 0;

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
                    sem_wait(write_semaphore);
                }
                // Чтение данных из БД
                int index = rand() % DATABASE_SIZE;
                int data = db->data[index];
                printf("Process %d read data from index %d: %d\n", getpid(), index, data);
                (db->num_r)--;
                if (db->num_r == 0)
                {
                    // Блокировка семафора для писателей
                    sem_post(write_semaphore);
                }
                sleep(1);
            }
            exit(0);
        }
    }
    (void)signal(SIGINT, my_handler);

    // Ожидание завершения дочерних процессов
    for (int i = 0; i < number_of_readers; i++)
    {
        wait(NULL);
    }
    return 0;
}