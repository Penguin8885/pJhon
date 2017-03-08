#define _CRT_SECURE_NO_WARNINGS     //Visual Studio セキュリティエラー非表示

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#define OUTPUT_NAME "output.html"     //出力ファイル名
#define FINENESS 100                    //画像の細かさ

#define CIRC_RSIZE ((int)(FINENESS*(4.0/5.0)))     //描く円の半径
#define FILTER_SR 0.75                                   //画像サイズに対するフィルターサイズの比率
#define FILTER_SIZE ((int)(FINENESS*FILTER_SR) + ((int)(FINENESS*FILTER_SR) + 1)%2) //フィルターサイズ

#define TH_NUM 11     //量子化数(レベル分割数)
double threshold[TH_NUM] = { -30, -35, -40, -45, -50, -55, -60, -65, -70, -75, -80 };     //閾値
char *color[TH_NUM] = { "ff0000", "ff6600", "ff9900", "ffff00", "99cc00", "006600", "00ccff", "3366ff", "330099", "330066", "330033" };     //電波強度分布のレベルごとの色

typedef struct _MAP{
     int dx;               //データ 横サイズ
     int dy;               //データ 縦サイズ
     int lx;               //レベルマップ 横サイズ
     int ly;               //レベルマップ 縦サイズ
     double **data;     //データ格納配列
     int **level;     //レベルマップ格納配列
} MAP;     //電波強度分布のマップ

/*強制終了関数*/
void err_exit(char *massage){
     fprintf(stderr, massage);
     fprintf(stderr, "... FAILURE\n");
     exit(EXIT_FAILURE);
     fflush(stdin);
     getchar();
}

/*データから電波レベルに変換する関数*/
int data2level(double data){
     int i;

     /*レベルは0からTH_NUM-1までで0が一番電波強度が強い*/
     for (i = 0; i < TH_NUM; i++){
          if (data > threshold[i]) return i;
     }
     return TH_NUM - 1;
}

/*CSVファイルからマップの原型を生成する関数*/
void input_fromCSV(MAP *map, char *filename){
     int i, j, m, n;
     int xcount, ycount;     //CSVのデータサイズを数えるための変数
     char buf[1024];          //CSVファイルの1行を読み込むためのバッファ
     char *tokp = NULL;     //バッファから切り出したトークンのポインタ
     FILE *fp = NULL;     //CSVファイルポインタ

     if ((fp = fopen(filename, "r")) == NULL) err_exit("cannot open the csv file\n");     //ファイルオープン

     /*データ数を数える*/
     xcount = ycount = 0;
     if (fgets(buf, sizeof(buf), fp) != NULL){
          ycount++;
          tokp = strtok(buf, ",");
          while (tokp != NULL){
               xcount++;
               tokp = strtok(NULL, ",");
          }
     }
     while (fgets(buf, sizeof(buf), fp) != NULL) ycount++;
     /*データ数から配列のサイズを計算*/
     map->dx = xcount;
     map->dy = ycount;
     map->lx = (xcount - 1) * FINENESS + 1;
     map->ly = (ycount - 1) * FINENESS + 1;

     fseek(fp, 0, SEEK_SET);

     /*データ数からマップを作り，データからレベルに変換する*/
     /*配列のメモリ確保*/
     if ((map->data = (double**)calloc(map->dx, sizeof(double*))) == NULL) err_exit("cannot allocate the memory\n");
     for (i = 0; i < map->dx; i++) if ((map->data[i] = (double*)calloc(map->dy, sizeof(double))) == NULL) err_exit("cannot allocate the memory\n");
     if ((map->level = (int**)calloc(map->lx, sizeof(int*))) == NULL) err_exit("cannot allocate the memory\n");
     for (i = 0; i < map->lx; i++) if ((map->level[i] = (int*)calloc(map->ly, sizeof(double))) == NULL) err_exit("cannot allocate the memory\n");
     /*データの場所に対応するマップの位置にそのデータのレベルを埋め込む*/
     for (i = 0, m = 0; i < map->dy; i++, m += FINENESS){
          if (fgets(buf, sizeof(buf), fp) == NULL) err_exit("fgets returns null\n");
          for (j = 0, n = 0; j < map->dx; j++, n += FINENESS){
               if (j != 0){
                    map->data[j][i] = atof(strtok(NULL, ","));
               }
               else{
                    map->data[j][i] = atof(strtok(buf, ","));
               }
               map->level[n][m] = data2level(map->data[j][i]);
          }
     }

     fclose(fp);     //ファイルクローズ
}

/*円を描くための関数*/
void draw_circ(MAP *map, int pox, int poy, int level){
     int x, y;

     for (x = pox - CIRC_RSIZE; x <= pox + CIRC_RSIZE; x++){
          if (x < 0 || x >= map->lx) continue;     //マップ外の座標のとき
          for (y = poy - CIRC_RSIZE; y <= poy + CIRC_RSIZE; y++){
               if (y < 0 || y >= map->ly) continue;     //マップ外の座標の時
               if ((x - pox)*(x - pox) + (y - poy)*(y - poy) <= (CIRC_RSIZE)*(CIRC_RSIZE)) {
                    map->level[x][y] = level;     //今計算している点の座標が円の中ならその点のレベルを書き換える
               }
          }
     }
}

/*移動平均フィルター*/
int MovingAverageFilter(int **array, int x, int y){
     int i, j, m, n;
     double **ext = NULL;     //サイズを拡張した配列
     int ex, ey;                    //拡張した配列の縦横サイズ
     double c = 1.0 / (FILTER_SIZE*FILTER_SIZE);     //移動平均の各点の係数
     double sum;

     int ffalf = FILTER_SIZE / 2;     //フィルターサイズの半分

     /*拡張した配列のサイズ*/
     ex = x + ffalf * 2;
     ey = y + ffalf * 2;

     /*メモリ確保*/
     if ((ext = (double**)calloc(ex, sizeof(double*))) == NULL) return EXIT_FAILURE;
     for (i = 0; i < ex; i++) if ((ext[i] = (double*)calloc(ey, sizeof(double))) == NULL) return EXIT_FAILURE;

     /*拡張部分に値を詰める*/
     for (i = 0; i < x; i++){
          for (j = 0; j < ffalf; j++){
               ext[i + ffalf][j] = array[i][0];
               ext[i + ffalf][ey - j - 1] = array[i][y - 1];
          }
     }
     for (i = 0; i < y; i++){
          for (j = 0; j < ffalf; j++){
               ext[j][i + ffalf] = array[0][i];
               ext[ex - 1 - j][i + ffalf] = array[x - 1][i];
          }
     }

     /*拡張部分(角)に値を詰める*/
     for (i = 0; i < ffalf; i++){
          for (j = 0; j < ffalf; j++){
               ext[i][j] = array[0][0];
               ext[ex - 1 - i][j] = array[x - 1][0];
               ext[i][ey - 1 - j] = array[0][y - 1];
               ext[ex - 1 - i][ey - 1 - j] = array[x - 1][y - 1];
          }
     }

     /*拡張部分以外はそのままの値を詰める*/
     for (i = 0; i < x; i++){
          for (j = 0; j < y; j++){
               ext[i + ffalf][j + ffalf] = array[i][j];
          }
     }

     /*フィルターをかけてもとの配列に入れる*/
     for (i = 0; i < x; i++){
          for (j = 0; j < y; j++){
               sum = 0;
               for (m = 0; m < FILTER_SIZE; m++){
                    for (n = 0; n < FILTER_SIZE; n++){
                         sum += c * ext[i + m][j + n];
                    }
               }
               array[i][j] = (int)(sum + 0.5);
          }
     }

     /*メモリ解放*/
     for (i = 0; i < ex; i++) free(ext[i]);
     free(ext);

     return EXIT_SUCCESS;
}

/*マップの値を計算する関数*/
void calc_maplevel(MAP *map){
     int i, j, k;

     /*下のレイヤーから円を描いていく*/
     for (i = 6; i >= 0; i--){
          for (j = 0; j < map->lx; j += FINENESS){
               for (k = 0; k < map->ly; k += FINENESS){
                    if (map->level[j][k] == i) {
                         draw_circ(map, j, k, i);
                    }
               }
          }
     }

     /*移動平均フィルターでマップを滑らかにする*/
     if(MovingAverageFilter(map->level, map->lx, map->ly) == EXIT_FAILURE) err_exit("MovingAcerageFilter Error\n");
}

/*マップをHTMLファイルとして出力する関数*/
void output_toHTML(MAP map, char *filename){
     int i, j;
     FILE *fp = NULL;     //HTMLファイルポインタ

     if ((fp = fopen(filename, "w")) == NULL) err_exit("cannot open the html file\n");     //ファイルオープン

     /*ヘッダー部分*/
     fprintf(fp, "<!DOCTYPE HTML>\n");
     fprintf(fp, "<html lang=\"ja\">\n");
     fprintf(fp, "<head>\n");
     fprintf(fp, "\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n");
     fprintf(fp, "\t<title>Radio wave intensity distribution chart</title>\n");
     /*スタイルシート*/
     fprintf(fp, "\t<style type=\"text/css\">\n");
     fprintf(fp, "\t\t.row { display: table; }\n");
     fprintf(fp, "\t\t.row > div { width: 1px; height: 1px; display: table-cell; }\n");
     fprintf(fp, "\t\t.liner { width: %dpx; height: 1px; display: table; background-color: #000000; }\n", map.lx + map.dx - 1);
     fprintf(fp, "\t\t.linec { background-color: #000000; }\n");
     for (i = 0; i < TH_NUM; i++){
          fprintf(fp, "\t\t.lv%d { background-color: #%s; }\n", i, color[i]);
     }
     fprintf(fp, "\t</style>\n");
     fprintf(fp, "</head>\n");
     fprintf(fp, "<body>\n");

     /*ボディ部分*/
     /*一番右と下のビットは枠線の関係上表示しないようになっています*/
     for (i = 0; i < map.ly - 1; i++){
          if ((i % FINENESS) == 0) fprintf(fp, "<div class=\"liner\"></div>\n");     //枠線(横)を描く
          fprintf(fp, "\t<div class=\"row\">");
          for (j = 0; j < map.lx - 1; j++){
               if ((j % FINENESS) == 0) fprintf(fp, "<div class=\"linec\"></div>");     //枠線(縦)を描く
               fprintf(fp, "<div class=\"lv%d\"></div>", map.level[j][i]);
          }
          fprintf(fp, "<div class=\"linec\"></div>");     //一番右の枠線(縦)を描く
          fprintf(fp, "</div>\n");
     }
     fprintf(fp, "<div class=\"liner\"></div>\n");     //一番下の枠線(横)を描く

     fprintf(fp, "</body>\n");
     fprintf(fp, "</html>");

     fclose(fp);     //ファイルクローズ
}

/*マップのメモリを開放する関数*/
void free_map(MAP *map){
     int i;

     for (i = 0; i < map->dx; i++) free(map->data[i]);
     free(map->data);
     for (i = 0; i < map->lx; i++) free(map->level[i]);
     free(map->level);
}

int main(int argc, char **argv){
     MAP map = { 0, 0, 0, 0, NULL, NULL };

     if (argc != 2) err_exit("drag & drop CSV file\n");     //CSVファイルをコマンドライン引数としないとき
     puts("Now Calculating...");
     input_fromCSV(&map, argv[1]);          //CSVファイルから読み込み
     calc_maplevel(&map);                    //マップの値を計算
     output_toHTML(map, OUTPUT_NAME);     //HTMLファイルに出力
     free_map(&map);                              //メモリ解放
     puts("SUCCESS");
     return EXIT_SUCCESS;
}
