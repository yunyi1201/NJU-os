#include "thread.h"
#include "thread-sync.h"

#include <assert.h>
#include <stdbool.h>

#define N 3

mutex_t lk = MUTEX_INIT();
cond_t cv = COND_INIT();

bool has_fork[N] = {false};

void Tphilospher_eat(int id) {
  printf("tphilosopher %d eat\n", id);
}

void Tphilosopher(int id) {
  int lhs = (N + id - 1) % N;
  int rhs = id % N;
  mutex_lock(&lk);
  while (!has_fork[lhs] && has_fork[rhs]) {
    cond_wait(&cv, &lk);
  }
  Tphilospher_eat(id);
  has_fork[lhs] = has_fork[rhs] = false;
  mutex_unlock(&lk);   

  mutex_lock(&lk);
  has_fork[lhs] = has_fork[rhs] = true;
  cond_broadcast(&cv);
  mutex_unlock(&lk);
}

int main(int argc, char *argv[]) {
  for (int i = 0; i < N; i++) {
    create(Tphilosopher);
  }
}
