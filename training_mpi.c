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
	srand(rank+1);

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	if (rank == PLAYERNUM) {
		int inmsg[2], outmsg[2];
		Field myField;
		resetField(&myField);
		for (i=0; i<ROUNDS; i++) {//900 rounds
		
			myField.old_ball_coord[0] = myField.ball_coord[0];
			myField.old_ball_coord[1] = myField.ball_coord[1];
		
			printf("%d\n", i+1);//print round number
			printf("%d %d\n", myField.ball_coord[0], myField.ball_coord[1]);//print ball coord
		
			for (j=0; j<PLAYERNUM; j++) {//for each player
			
				outmsg[0] = myField.ball_coord[0];//tell them where the ball is
				outmsg[1] = myField.ball_coord[1];
				rc = MPI_Send(outmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD);
				
				rc = MPI_Recv(inmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);//remember player old coordinate
				myField.old_players_coord[2*j] = inmsg[0];
				myField.old_players_coord[2*j+1] = inmsg[1];
				
				rc = MPI_Recv(inmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);//remember player new coordinate
				myField.new_players_coord[2*j] = inmsg[0];
				myField.new_players_coord[2*j+1] = inmsg[1];
				
				
			}
			
			winner_id = getWinner(&myField);//find winner of the ball
			
			for (j=0; j<PLAYERNUM; j++) {//for each player
			
				outmsg[0] = 0;//tell them they win ball or not
				if (j == winner_id) {outmsg[0] = 1;}
				rc = MPI_Send(outmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
				
				rc = MPI_Recv(inmsg, 2, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);//get where they kick towards
				
				if (j == winner_id) {//but only remember if they are the ball winner
					myField.ball_coord[0] = inmsg[0];
					myField.ball_coord[1] = inmsg[1];
				}
			}
			
			for (j=0; j<PLAYERNUM; j++) {//for each player
				printf("%d ", j);//print player id
				printf("%d %d ", myField.old_players_coord[2*j], myField.old_players_coord[2*j+1]);	//print old coord
				printf("%d %d ", myField.new_players_coord[2*j], myField.new_players_coord[2*j+1]);	//print new coord
				
				if (myField.new_players_coord[2*j] == myField.old_ball_coord[0] && //print reach ball or not
					myField.new_players_coord[2*j+1] == myField.old_ball_coord[1]) {
					printf("1 ");
				} else {
					printf("0 ");
				}
				
				if (j == winner_id) {printf("1 ");} else {printf("0 ");}//print win the ball or not
				
				rc = MPI_Recv(inmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);//ask and print how many distance
				printf("%d ", inmsg[0]);
				rc = MPI_Recv(inmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);//ask and print how many times reach ball
				printf("%d ", inmsg[0]);
				rc = MPI_Recv(inmsg, 1, MPI_INT, j, tag, MPI_COMM_WORLD, &Stat);//ask and print how many times kick ball
				printf("%d\n", inmsg[0]);
			}
		}
	} else {
		int inmsg[2], outmsg[2];
		Player myPlayer;
		initPlayer(&myPlayer);
		for (i=0; i<ROUNDS; i++) {
			rc = MPI_Recv(inmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD, &Stat);				//receive ball position
			
			outmsg[0] = myPlayer.coord[0];															//send initial position
			outmsg[1] = myPlayer.coord[1];
			rc = MPI_Send(outmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			
			myPlayer.total_distance += Run(&myPlayer, inmsg, myPlayer.coord);						//Run to the ball position
			
			outmsg[0] = myPlayer.coord[0];															//send current position
			outmsg[1] = myPlayer.coord[1];
			rc = MPI_Send(outmsg, 2, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);
			if (inmsg[0] == outmsg[0] && inmsg[1] == outmsg[1]) {
				myPlayer.total_reach++;
			}
			
			rc = MPI_Recv(inmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD, &Stat);				//receive whether win ball or not
			if (inmsg[0] == 1) {
				myPlayer.total_kick++;
			}
			
			Kick(&myPlayer, outmsg);																//Kick the imaginary ball (or real ball if lucky)
			
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);						//Send intended ball location
			outmsg[0] = myPlayer.total_distance;
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);						//send how many distance covered
			
			outmsg[0] = myPlayer.total_reach;
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);						//send how many times reach ball
			
			outmsg[0] = myPlayer.total_kick;
			rc = MPI_Send(outmsg, 1, MPI_INT, PLAYERNUM, tag, MPI_COMM_WORLD);						//send how many times kick ball
			if (rank == 1) {
					//printf("%d %d\n", myPlayer.coord[0], myPlayer.coord[1]);
			}
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
int initPlayer(Player* myPlayer) {
	myPlayer->coord[0] = rand()%LENGTH;
	myPlayer->coord[1] = rand()%WIDTH;
	myPlayer->total_distance = 0;
	myPlayer->total_reach = 0;
	myPlayer->total_kick = 0;
	return 0;
}

int Run(Player* myPlayer, int* towards, int* result) {
	int x = towards[0] - myPlayer->coord[0];
	int y = towards[1] - myPlayer->coord[1];
	//printf("%d %d \n", x, y);
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
int Kick(Player* myPlayer, int* result) {
	result[0] = rand()%LENGTH;
	result[1] = rand()%WIDTH;
	return 0;
}