//DB이름 : projectDb
//세글자단어 table 이름 : wordTb
//라운드단어 table 이름 : roundTb
//필드 이름 : word

#include <mysql.h>
#include <stdlib.h>

MYSQL mysql;

void mysqlInit()
{
	mysql_init(&mysql);
}

void mysqlConnect(char *server, char *user, char *password, char *database, int port)
{
	if(!mysql_real_connect(&mysql, server, user, password, database, port, (char *)NULL, 0))
	{
		printf("errno = %u\n", mysql_errno(&mysql));
		printf("error message = %s\n", mysql_error(&mysql)); //DB접속 실패
	}
	else
	{
		printf("DB 연결성공\n"); // DB접속 성공
	}
}

char* roundWord() //라운드 제시어 가져옴(제시어 포인터반환)
{
	MYSQL_RES *res;
	MYSQL_ROW row;

	char query[500] = {0,};
    char* roundTemp = (char*)malloc(sizeof(char)*19);
    memset(roundTemp, 0, sizeof(roundTemp));

	int fields;

	strcpy(query, "select * from roundTb order by rand() limit 1");

	if(mysql_query(&mysql, query))
	{
		printf("errno = %u\n", mysql_errno(&mysql));
		printf("error message = %s\n", mysql_error(&mysql));
	}

	res = mysql_store_result(&mysql);
	fields = mysql_num_fields(res);
	row = mysql_fetch_row(res);

	if(row != NULL)
	{
		sprintf(roundTemp,"%s",row[0]);
		return roundTemp;
	}
	else
	{
		return NULL;
	}
	
	mysql_free_result(res);
}


int wordCompare(char *clientWord) // 단어일치 유무 검사 파라미터로 세글자단어(일치하면 1,아니면 0반환)
{
	MYSQL_RES *res;
	MYSQL_ROW row;

	char query[500] = {0,};
    char temp[21] = {0,};

	int fields;

	sprintf(query, "select word from wordTb where word = '%s'", clientWord);
   
	if(mysql_query(&mysql, query))
	{
		printf("errno = %u\n", mysql_errno(&mysql));
		printf("error message = %s\n", mysql_error(&mysql));
	}

	res = mysql_store_result(&mysql);
	fields = mysql_num_fields(res);
	row = mysql_fetch_row(res);

	if(row != NULL)
	{
		sprintf(temp,"%s",row[0]);

		if(strncmp(temp,clientWord,9) == 0)
		{
            printf("%s\n",temp);
			return 1; // 단어 일치
		}
		else
		{
			return 0; //단어 불일치
		}
	}
    else{
        printf("ㅅㅂ\n");
        return 0;
    }
	mysql_free_result(res);
}

char* wordHint(char *clientWord) // hint 전송, 파라미터로 마지막글자 받음(힌트 포인터 반환) 
{
	MYSQL_RES *res;
	MYSQL_ROW row;

	char query[500] = {0,};
    char* wordTemp = (char*)malloc(sizeof(char)*10);
    memset(wordTemp, 0, sizeof(wordTemp));

	int fields;

	sprintf(query, "select * from wordTb where word like '%s%%' order by rand() limit 1", clientWord);
	
	if(mysql_query(&mysql, query))
	{
		printf("errno = %u\n", mysql_errno(&mysql));
		printf("error message = %s\n", mysql_error(&mysql));
	}

	res = mysql_store_result(&mysql);
	fields = mysql_num_fields(res);
	row = mysql_fetch_row(res);

	if(row != NULL)
	{
		sprintf(wordTemp,"%s",row[0]);
		return wordTemp;
	}
	else
	{
		return NULL; //힌트 없음
	}
	
	mysql_free_result(res);
}



void mysqlQuit()
{
	mysql_close(&mysql);
}
