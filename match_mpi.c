#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>


//Player structure, can run, kick, and challenge
typedef struct {
	int coord[2];
	int speed, dribbling, kick, total_distance, total_reach, total_kick;
} Player;
int initPlayer(Player*);
int Run(Player*, int*, int*);
int Kick(Player*, int*, int*);
int Challenge(Player*);

#define PLAYERNUM 22
#define LENGTH 128
#define WIDTH 96
#define FIELDNUM 12
//Field structure, can find winner of ball contest
typedef struct {
	int ball_coord[2];
	int old_players_coord[2 * PLAYERNUM];
	int new_players_coord[2 * PLAYERNUM];
	int challengers[PLAYERNUM];
	int old_ball_coord[2];
} Field;
int resetField(Field*);
int getWinner(Field*);

#define NPROCS 34
#define FIELD 0
#define TEAM_A 1
#define TEAM_B 2
#define ROUNDS 2700
int main(int argc,char *argv[]) {
	int rank, new_rank, sendbuf, recvbuf, numtasks, color, i, j;
	MPI_Comm   team_comm;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	
	srand(rank+1);
	
	//set colors
	if (rank < 12) {
		color = FIELD;
	} else if (rank < 23) {
		color = TEAM_A;
	} else {
		color = TEAM_B;
	}

	if (numtasks != NPROCS) {
		printf("Must specify MP_PROCS= %d. Terminating.\n",NPROCS);
		MPI_Finalize();
		exit(0);
	}

	//Divide processes into 3 communicators based on team. The fields are considered 1 team
	MPI_Comm_split(MPI_COMM_WORLD, color, rank, &team_comm);
	
	if (rank < 12) {
		int source, buffer[2*NPROCS], winner_id;
		Field myField;
		resetField(&myField);
		//for each round
		for (i=0; i<2*ROUNDS; i++) {
			
			//Synchronise old_ball_coord with ball_coord
			myField.old_ball_coord[0] = myField.ball_coord[0];
			myField.old_ball_coord[1] = myField.ball_coord[1];
			
			//only for field 0
			if (rank == 0) {
				//print the round number
				printf("%d\n", i+1);
				
				//print ball coord
				printf("%d %d\n", myField.ball_coord[0], myField.ball_coord[1]);
			}
			
			//broadcast the position of the ball to every single process, the broadcaster is field 0
			buffer[0] = myField.ball_coord[0];
			buffer[1] = myField.ball_coord[1];
			MPI_Bcast(buffer, 2, MPI_INT, 0, MPI_COMM_WORLD);
			
			//afterwards, the source is always the field that has the ball
			source = myField.ball_coord[0]/32 + 4 * (myField.ball_coord[1]/32);
			
			//Gather all the players old position to the source field
			MPI_Gather(buffer, 2, MPI_INT, buffer, 2, MPI_INT, source, MPI_COMM_WORLD);
			
			//source remember all players old position
			if (rank == source) {
				for (j=0; j<2*PLAYERNUM; j++) {
					myField.old_players_coord[j] = buffer[j+2*FIELDNUM];
				}
			}
			
			//Gather all the player new positions
			MPI_Gather(buffer, 2, MPI_INT, buffer, 2, MPI_INT, source, MPI_COMM_WORLD);
			
			//source remember all players new position
			if (rank == source) {
				for (j=0; j<2*PLAYERNUM; j++) {
					myField.new_players_coord[j] = buffer[j+2*FIELDNUM];
				}
			}
			
			//gather all player challenge
			MPI_Gather(buffer, 1, MPI_INT, buffer, 1, MPI_INT, source, MPI_COMM_WORLD);
			
			//remember the challengers value
			if (rank == source) {
				for (j=0; j<PLAYERNUM; j++) {
					if (myField.new_players_coord[2*j] == myField.ball_coord[0] && myField.new_players_coord[2*j+1] == myField.ball_coord[1]) {
						myField.challengers[j] = buffer[j+FIELDNUM];
					} else {
						myField.challengers[j] = -1;
					}
				}
			}
			
			//find the winner of contest
			winner_id = getWinner(&myField);
			
			//Broadcast the winner
			buffer[0] = winner_id;
			MPI_Bcast(buffer, 1, MPI_INT, source, MPI_COMM_WORLD);
			
			//gather all intended kick target
			MPI_Gather(buffer, 2, MPI_INT, buffer, 2, MPI_INT, source, MPI_COMM_WORLD);
			
			//remember intended kick target only from the winner
			if (winner_id >=0) {
				myField.ball_coord[0] = buffer[2*winner_id];
				myField.ball_coord[1] = buffer[2*winner_id+1];
			}
			
			//if ball is out of bound, reset field
			if (myField.ball_coord[0] > LENGTH - 1 || myField.ball_coord[0] < 0 || myField.ball_coord[1] > WIDTH - 1 || myField.ball_coord[1] < 0) {
				resetField(&myField);
			}
			
			//broadcast and synchronise all fields information
			MPI_Bcast(myField.old_players_coord, 2*PLAYERNUM, MPI_INT, source, team_comm);
			MPI_Bcast(myField.new_players_coord, 2*PLAYERNUM, MPI_INT, source, team_comm);
			MPI_Bcast(myField.ball_coord, 2, MPI_INT, source, team_comm);
			MPI_Bcast(myField.challengers, PLAYERNUM, MPI_INT, source, team_comm);
			
			//broadcast the winner to all field as well
			buffer[0] = winner_id;
			MPI_Bcast(buffer, 1, MPI_INT, source, team_comm);
			
			//remember who is winning
			winner_id = buffer[0];
			
			//for only field 0
			if (rank == 0) {
				
				//for each player
				for (j=0; j<PLAYERNUM; j++) {
					
					//print player id
					printf("%d ", j % 11);
					
					//print old coord
					printf("%d %d ", myField.old_players_coord[2*j], myField.old_players_coord[2*j+1]);
					
					//print new coord
					printf("%d %d ", myField.new_players_coord[2*j], myField.new_players_coord[2*j+1]);
					
					//print reach ball or not
					if (myField.new_players_coord[2*j] == myField.old_ball_coord[0] && 
						myField.new_players_coord[2*j+1] == myField.old_ball_coord[1]) {
						printf("1 ");
					} else {
						printf("0 ");
					}
					
					//print win the ball or not
					if (j+FIELDNUM == winner_id) {printf("1 ");} else {printf("0 ");}
					
					//print ball challenge
					printf("%d\n", myField.challengers[j]);
				}
			}
		}
	} else {
		int inmsg[2], outmsg[2], target[2], source;
		Player myPlayer;
		initPlayer(&myPlayer);
		
		//for each rounds
		for (i=0; i<2*ROUNDS; i++) {
			
			//get ball coord from field 0
			MPI_Bcast(inmsg, 2, MPI_INT, 0, MPI_COMM_WORLD);
			
			//afterwards, the source is always the field that has the ball
			source = inmsg[0]/32 + 4 * (inmsg[1]/32);
			
			//send initial position to field source
			outmsg[0] = myPlayer.coord[0];
			outmsg[1] = myPlayer.coord[1];
			MPI_Gather(outmsg, 2, MPI_INT, NULL, 2, MPI_INT, source, MPI_COMM_WORLD);
			
			//TODO specify where to run
			//In the end, they just chase the ball no matter what
			target[0] = inmsg[0];
			target[1] = inmsg[1];
			
			//run to the ball position
			myPlayer.total_distance += Run(&myPlayer, target, myPlayer.coord);
			
			//send new position to field source
			outmsg[0] = myPlayer.coord[0];
			outmsg[1] = myPlayer.coord[1];
			MPI_Gather(outmsg, 2, MPI_INT, NULL, 2, MPI_INT, source, MPI_COMM_WORLD);
			if (inmsg[0] == outmsg[0] && inmsg[1] == outmsg[1]) {
				myPlayer.total_reach++;
			}
			
			//send challenge to field source
			outmsg[0] = Challenge(&myPlayer);
			MPI_Gather(outmsg, 1, MPI_INT, NULL, 1, MPI_INT, source, MPI_COMM_WORLD);
			
			//receive who win
			MPI_Bcast(inmsg, 1, MPI_INT, source, MPI_COMM_WORLD);
			
			if (inmsg[0] == rank) {
				myPlayer.total_kick++;
			}
			
			//TODO specify where to kick
			//In the end, they just kick towards the goal
			if (rank < 23) {
				if (i<ROUNDS) {
					target[0] = -1;
				} else {
					target[0] = LENGTH;
				}
			} else {
				if (i<ROUNDS) {
					target[0] = LENGTH;
				} else {
					target[0] = -1;
				}
			}
			target[1] = WIDTH/2;
			
			//put where the player want to kick towards in outmsg
			Kick(&myPlayer, target, outmsg);
			
			//send where the intended ball destination to field source
			MPI_Gather(outmsg, 2, MPI_INT, NULL, 2, MPI_INT, source, MPI_COMM_WORLD);
		}
	}
	MPI_Finalize();
	return 0;
}

int resetField(Field* myField) {
	myField->ball_coord[0] = LENGTH/2;
	myField->ball_coord[1] = WIDTH/2;
	return 0;
}

int getWinner(Field* myField) {
	int totalMaxChallenger = 0;
	int MaxChallengers[PLAYERNUM];
	int j;
	float maxChallenge = -0.5;
	for (j=0; j<PLAYERNUM; j++) {
		if (myField->challengers[j] >= maxChallenge) {
			maxChallenge = (float)(myField->challengers[j]);
		}
	}
	for (j=0; j<PLAYERNUM; j++) {
		if (myField->challengers[j] == maxChallenge) {
			MaxChallengers[totalMaxChallenger] = j;
			totalMaxChallenger++;
		}
	}
	if (totalMaxChallenger <= 0) {
		return -1;
	}
	return MaxChallengers[rand()%totalMaxChallenger]+FIELDNUM;
}

int initPlayer(Player* myPlayer) {
	myPlayer->coord[0] = rand()%LENGTH;
	myPlayer->coord[1] = rand()%WIDTH;
	myPlayer->speed = 1 + rand()%10;
	myPlayer->dribbling = 1 + rand()%(14 - myPlayer->speed);
	myPlayer->kick = 15 - myPlayer->speed - myPlayer->dribbling;
	return 0;
}

int Run(Player* myPlayer, int* towards, int* result) {
	int x = towards[0] - myPlayer->coord[0];
	int y = towards[1] - myPlayer->coord[1];
	int maxDistance = 10;
	if (maxDistance > myPlayer->speed) {
		maxDistance = myPlayer->speed;
	}
	if (abs(x) <= maxDistance) {
		myPlayer->coord[0] += x;
		result[0] = myPlayer->coord[0];
		if (abs(x) + abs(y) <= maxDistance) {
			myPlayer->coord[1] += y;
			result[1] = myPlayer->coord[1];
		} else if (y >= 0){
			myPlayer->coord[1] += maxDistance-abs(x);
			result[1] = myPlayer->coord[1];
		} else {
			myPlayer->coord[1] -= maxDistance+abs(x);
			result[1] = myPlayer->coord[1];
		}
	} else if (x >= 0) {
		myPlayer->coord[0] += maxDistance;
		result[0] = myPlayer->coord[0];
		result[1] = myPlayer->coord[1];
	} else {
		myPlayer->coord[0] -= maxDistance;
		result[0] = myPlayer->coord[0];
		result[1] = myPlayer->coord[1];
	}
	if (abs(x) + abs(y) > maxDistance) {
		return maxDistance;
	} else {
		return abs(x) + abs(y);
	}
}

int Kick(Player* myPlayer, int* towards, int* result) {
	int x = towards[0] - myPlayer->coord[0];
	int y = towards[1] - myPlayer->coord[1];
	if (abs(y) <= 2*myPlayer->kick) {
		result[1] = myPlayer->coord[1] + y;
		if (abs(x) + abs(y) <= 2*myPlayer->kick) {
			result[0] = myPlayer->coord[0] + x;
		} else if (x >= 0){
			result[0] = myPlayer->coord[0] + 2*myPlayer->kick - abs(y);
		} else {
			result[0] = myPlayer->coord[0] - 2*myPlayer->kick + abs(y);
		}
	} else if (y >= 0) {
		result[1] = myPlayer->coord[1] + 2*myPlayer->kick;
		result[0] = myPlayer->coord[0];
	} else {
		result[1] = myPlayer->coord[1] - 2*myPlayer->kick;
		result[0] = myPlayer->coord[0];
	}
	if (abs(x) + abs(y) > 2*myPlayer->kick) {
		return 2*myPlayer->kick;
	} else {
		return abs(x) + abs(y);
	}
}

int Challenge(Player* myPlayer) {
	return (rand()%10 + 1) * myPlayer->dribbling;
}