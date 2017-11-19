#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <math.h>


//Player structure, can run and kick
typedef struct {
	int coord[2];
	int total_distance;
	int total_reach;
	int total_kick;
} Player;
int initPlayer(Player*);
int Run(Player*, int*, int*);
int Kick(Player*, int*);

#define PLAYERNUM 11
#define LENGTH 128
#define WIDTH 64
//Field structure, can find winner of ball contest
typedef struct {
	int ball_coord[2];
	int old_players_coord[2 * PLAYERNUM];
	int new_players_coord[2 * PLAYERNUM];
	int old_ball_coord[2];
} Field;
int resetField(Field*);
int getWinner(Field*);


#define ROUNDS 900
int main(int argc,char *argv[]) {
	int numtasks, rank, dest, source, rc, count, tag=1, i, j, winner_id;  
	MPI_Status Stat;
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	srand(time(NULL)+rank);
	
	//Setting seed for debugging purpose, uncomment for consistent output
	//srand(rank+1);

	if (rank == PLAYERNUM) {
		int inmsg[2], outmsg[2];
		Field myField;
		resetField(&myField);
		
		//for each round
		for (i=0; i<ROUNDS; i++) {
			
			//Synchronise old_ball_coord with ball_coord
			myField.old_ball_coord[0] = myField.ball_coord[0];
			myField.old_ball_coord[1] = myField.ball_coord[1];
		
			//print the round number
			printf("%d\n", i+1);
			
			//print ball coord
			printf("%d %d\n", myField.ball_coord[0], myField.ball_coord[1]);
			
			//for each player
			for (j=0; j<PLAYERNUM; j++) {
				
				//tell them where the ball is
				outmsg[0] = myField.ball_coord[0];
				outmsg[1] = myField.ball_coord[1];
				rc = MPI_Send(outmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD);
				
				//ask and store player old coordinate
				rc = MPI_Recv(inmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);
				myField.old_players_coord[2*j] = inmsg[0];
				myField.old_players_coord[2*j+1] = inmsg[1];
				
				//ask and store player new coordinate
				rc = MPI_Recv(inmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);
				myField.new_players_coord[2*j] = inmsg[0];
				myField.new_players_coord[2*j+1] = inmsg[1];
			}
			
			//find the winner of contest
			winner_id = getWinner(&myField);
			
			//for each player
			for (j=0; j<PLAYERNUM; j++) {
				
				//tell them they win ball or not
				outmsg[0] = 0;
				if (j == winner_id) {outmsg[0] = 1;}
				rc = MPI_Send(outmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
				
				//get where they want to kick towards
				rc = MPI_Recv(inmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);
				
				//but only store if they are the ball winner
				if (j == winner_id) {
					myField.ball_coord[0] = inmsg[0];
					myField.ball_coord[1] = inmsg[1];
				}
			}
			
			//for each player
			for (j=0; j<PLAYERNUM; j++) {
				
				//print player id
				printf("%d ", j);
				
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
				if (j == winner_id) {printf("1 ");} else {printf("0 ");}
				
				//ask and print how many distance
				rc = MPI_Recv(inmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);
				printf("%d ", inmsg[0]);
				
				//ask and print how many times reach ball
				rc = MPI_Recv(inmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);
				printf("%d ", inmsg[0]);
				
				//ask and print how many times kick ball
				rc = MPI_Recv(inmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);
				printf("%d\n", inmsg[0]);
			}
		}
	} else {
		int inmsg[2], outmsg[2];
		Player myPlayer;
		initPlayer(&myPlayer);
		
		//for each rounds
		for (i=0; i<ROUNDS; i++) {
			
			//get ball coord from field
			rc = MPI_Recv(inmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD, &Stat);
			
			//send initial position to field
			outmsg[0] = myPlayer.coord[0];
			outmsg[1] = myPlayer.coord[1];
			rc = MPI_Send(outmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			
			//run to the ball position
			myPlayer.total_distance += Run(&myPlayer, inmsg, myPlayer.coord);
			
			//send new position to field
			outmsg[0] = myPlayer.coord[0];
			outmsg[1] = myPlayer.coord[1];
			rc = MPI_Send(outmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			if (inmsg[0] == outmsg[0] && inmsg[1] == outmsg[1]) {
				myPlayer.total_reach++;
			}
			
			//receive whether win ball or not
			rc = MPI_Recv(inmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD, &Stat);
			if (inmsg[0] == 1) {
				myPlayer.total_kick++;
			}
			
			//put where the player want to kick towards in outmsg
			Kick(&myPlayer, outmsg);
			
			//send where the intended ball destination to field
			rc = MPI_Send(outmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			
			//send how many distance covered so far to field
			outmsg[0] = myPlayer.total_distance;
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			
			//send how many times have reach the ball to field
			outmsg[0] = myPlayer.total_reach;
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			
			//send how many times kicked the ball to field
			outmsg[0] = myPlayer.total_kick;
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
		}
	}
	MPI_Finalize();
	return 0;
}

//put the ball in the middle
int resetField(Field* myField) {
	myField->ball_coord[0] = LENGTH/2;
	myField->ball_coord[1] = WIDTH/2;
	return 0;
}

//get random id out of players who reach the ball, if no one reach the ball, return -1
int getWinner(Field* myField) {
	int totalReaching = 0;
	int Reaching[PLAYERNUM];
	int j;
	for (j=0; j<PLAYERNUM; j++) {
		if (myField->new_players_coord[2*j] == myField->ball_coord[0] && myField->new_players_coord[2*j+1] == myField->ball_coord[1] ) {
			Reaching[totalReaching] = j;
			totalReaching++;
		}
	}
	if (totalReaching <= 0) {
		return -1;
	}
	return Reaching[rand()%totalReaching];
}

//set player to random position and reset distance, reach, kick counter
int initPlayer(Player* myPlayer) {
	myPlayer->coord[0] = rand()%LENGTH;
	myPlayer->coord[1] = rand()%WIDTH;
	myPlayer->total_distance = 0;
	myPlayer->total_reach = 0;
	myPlayer->total_kick = 0;
	return 0;
}

/*move the player closer to "towards" position, x axis movement prioritised,
store the resulting coordinate in "result" return distance covered */
int Run(Player* myPlayer, int* towards, int* result) {
	int x = towards[0] - myPlayer->coord[0];
	int y = towards[1] - myPlayer->coord[1];
	if (abs(x) <= 10) {
		myPlayer->coord[0] += x;
		result[0] = myPlayer->coord[0];
		if (abs(x) + abs(y) <= 10) {
			myPlayer->coord[1] += y;
			result[1] = myPlayer->coord[1];
		} else if (y >= 0){
			myPlayer->coord[1] += 10-abs(x);
			result[1] = myPlayer->coord[1];
		} else {
			myPlayer->coord[1] -= 10-abs(x);
			result[1] = myPlayer->coord[1];
		}
	} else if (x >= 0) {
		myPlayer->coord[0] += 10;
		result[0] = myPlayer->coord[0];
		result[1] = myPlayer->coord[1];
	} else {
		myPlayer->coord[0] -= 10;
		result[0] = myPlayer->coord[0];
		result[1] = myPlayer->coord[1];
	}
	if (abs(x) + abs(y) > 10) {
		return 10;
	} else {
		return abs(x) + abs(y);
	}
}

//store random coordinate in "result"
int Kick(Player* myPlayer, int* result) {
	result[0] = rand()%LENGTH;
	result[1] = rand()%WIDTH;
	return 0;
}