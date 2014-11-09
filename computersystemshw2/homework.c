/* 
 * file:        homework.c
 * description: Skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern CCIS, 2011
 * $Id: homework.c 530 2012-01-31 19:55:02Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"

/********** YOUR CODE STARTS HERE ******************/
typedef int bool;
#define true 1
#define false 0

const float AVG_HAIRCUT_TIME = 1.2;
const float AVG_CUSTOMER_SLEEP_TIME = 10.0;
const int SHOP_CAPACITY = 5;
const int NUMBER_OF_CUSTOMERS = 10;


bool barber_chair_empty = true;
int num_people_in_shop = 0;

bool BARBER_ASLEEP = true;

int total_customers_counter = 0;
int customers_turned_away_full_shop = 0;

void* time_in_shop_timer = NULL;
void* number_of_customers_in_shop_counter = NULL;
void* time_in_chair_counter = NULL;


/*
 * Here's how you can initialize global mutex and cond variables
 */
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t wake_barber = PTHREAD_COND_INITIALIZER;
pthread_cond_t barber_action = PTHREAD_COND_INITIALIZER;
pthread_cond_t sit_on_barber_chair = PTHREAD_COND_INITIALIZER;
pthread_cond_t get_off_barber_chair = PTHREAD_COND_INITIALIZER;

/* the barber method
 */
void barber(void)
{
    pthread_mutex_lock(&m);
    while (1) {
        // if barber just cut hair of last customer in the shop or if the shop is empty
        if((barber_chair_empty && num_people_in_shop == 0) || (!barber_chair_empty && num_people_in_shop == 1)) {
            BARBER_ASLEEP = true;
            printf("DEBUG: %f barber goes to sleep\n",timestamp()); 
            pthread_cond_wait(&wake_barber,&m);
            printf("DEBUG: %f barber wakes up\n",timestamp()); 
            pthread_cond_signal(&sit_on_barber_chair); 
        } 
        BARBER_ASLEEP = false;

        pthread_cond_wait(&barber_action, &m);
        //perform haircut
        sleep_exp(AVG_HAIRCUT_TIME,&m);
        //signal one customer
        pthread_cond_signal(&get_off_barber_chair); 

    }
    pthread_mutex_unlock(&m);
}

/* the customer method
 */
void customer(int customer_num)
{
    pthread_mutex_lock(&m);

    if(num_people_in_shop == SHOP_CAPACITY) {
        customers_turned_away_full_shop++;
        pthread_mutex_unlock(&m);
        return;
    }

    total_customers_counter++;
    printf("DEBUG: %f customer %d enters shop\n",timestamp(),customer_num); 
    num_people_in_shop++;
    stat_timer_start(time_in_shop_timer);
    stat_count_incr(number_of_customers_in_shop_counter);


    if(BARBER_ASLEEP) {
        //signal barber so he will wake up
        pthread_cond_signal(&wake_barber);
        //wait for barber to prepare chair
        pthread_cond_wait(&sit_on_barber_chair,&m);
    }
    while(!barber_chair_empty || BARBER_ASLEEP) {
        pthread_cond_wait(&sit_on_barber_chair,&m);
    }

    barber_chair_empty = false;
    printf("DEBUG: %f customer %d starts haircut\n",timestamp(),customer_num); 
    stat_count_incr(time_in_chair_counter);
    pthread_cond_signal(&barber_action);

    pthread_cond_wait(&get_off_barber_chair, &m);

    barber_chair_empty = true;
    num_people_in_shop--;
    stat_timer_stop(time_in_shop_timer);
    stat_count_decr(time_in_chair_counter);
    stat_count_decr(number_of_customers_in_shop_counter);

    printf("DEBUG: %f customer %d leaves shop\n",timestamp(),customer_num); 
    pthread_cond_signal(&sit_on_barber_chair);

    pthread_mutex_unlock(&m);
    return;
}

/* Threads which call these methods. Note that the pthread create
 * function allows you to pass a single void* pointer value to each
 * thread you create; we actually pass an integer (the customer number)
 * as that argument instead, using a "cast" to pretend it's a pointer.
 */

/* the customer thread function - create 10 threads, each of which calls
 * this function with its customer number 0..9
 */
void *customer_thread(void *context) 
{
    int customer_num = (int)context; 

    /* your code goes here */
    while(1) {
        sleep_exp(AVG_CUSTOMER_SLEEP_TIME,NULL);
        customer(customer_num);
    }

    return 0;
}

/*  barber thread
 */
void *barber_thread(void *context)
{
    barber(); /* never returns */
    return 0;
}

void q2(void)
{
    /* to create a thread:
        pthread_t t; 
        pthread_create(&t, NULL, function, argument);
       note that the value of 't' won't be used in this homework
    */

    /* your code goes here */
    pthread_t barber_t;
    pthread_create(&barber_t, NULL, barber_thread, NULL);

    pthread_t customer_t;
    int x = 0;
    for(; x<NUMBER_OF_CUSTOMERS; x++){
        pthread_create(&customer_t, NULL, customer_thread, (void*) x);
    }

    wait_until_done();
    return;
}

/* For question 3 you need to measure the following statistics:
 *
 * 1. fraction of  customer visits result in turning away due to a full shop 
 *    (calculate this one yourself - count total customers, those turned away)
 * 2. average time spent in the shop (including haircut) by a customer 
 *     *** who does not find a full shop ***. (timer)
 * 3. average number of customers in the shop (counter)
 * 4. fraction of time someone is sitting in the barber's chair (counter)
 *
 * The stat_* functions (counter, timer) are described in the PDF. 
 */

void q3(void)
{
    /* your code goes here */


    pthread_t barber_t;
    pthread_create(&barber_t, NULL, barber_thread, NULL);
    time_in_shop_timer = stat_timer();
    number_of_customers_in_shop_counter = stat_counter();
    time_in_chair_counter = stat_counter();

    pthread_t customer_t;
    int x = 0;
    for(; x<NUMBER_OF_CUSTOMERS; x++){
        pthread_create(&customer_t, NULL, customer_thread, (void*) x);
    }

    wait_until_done();

    printf("fraction of  customer visits result in turning away due to a full shop is: %f\n",
        customers_turned_away_full_shop*1.0/total_customers_counter);

    printf("average time spent in the shop (including haircut) by a customer who does not find a full shop is: %f\n",
        stat_timer_mean(time_in_shop_timer));

    printf("average number of customers in the shop (including anyone sitting in the barber's chair) is: %f\n",
        stat_count_mean(number_of_customers_in_shop_counter));

    printf("fraction of time someone is sitting in the barber's chair is: %f\n",
        stat_count_mean(time_in_chair_counter));

}
