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
    // Удаление именованных семафоров
    sem_unlink("write_semaphore");

    // Удаление разделяемой памяти
    close(shm);
    if (shm_unlink("shared-memory") == -1)
    {
        printf("Shared memory is absent\n");
        perror("shm_unlink");
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

    // Создание именованного семафора для синхронизации процессов-писателей
    sem_t *write_semaphore = sem_open("write_semaphore", O_CREAT, 0666, 1);

    // Создание разделяемой памяти для БД

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

    if (ftruncate(shm, mem_size) == -1)
    {
        printf("Memory sizing error\n");
        perror("ftruncate");
        return 1;
    }
    else
    {
        printf("Memory size set and = %d\n", mem_size);
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
                sem_wait(write_semaphore);
                // Изменение БД
                int index = rand() % DATABASE_SIZE;
                db->data[index] = rand() % 100;
                printf("Process %d wrote data to index %d\n", getpid(), index);
                qsort(&db->data, DATABASE_SIZE, sizeof(int), cmp);
                // Разблокировка семафора для писателей
                sem_post(write_semaphore);
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
    // обработка сигнала прерывания
    (void)signal(SIGINT, my_handler);

    // Ожидание завершения дочерних процессов
    for (int i = 0; i < number_of_readers + number_of_writers; i++)
    {
        wait(NULL);
    }
    return 0;
}