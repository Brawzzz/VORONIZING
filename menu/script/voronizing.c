//==================================== INCLUDES ====================================//
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <jpeglib.h>
#include <png.h>

//==================================== STRUCTURES ====================================//
typedef struct{
    float x;
    float y;
    float z;
}CVector;

typedef CVector* PCVector;

typedef struct{
    int red;
    int green;
    int blue;
    int alpha;
}Color_RGBA;

typedef struct{
    CVector c_vector;
    Color_RGBA color;
}Seed;

typedef Seed* PSeed;

//==================================== CONSTANTS ====================================//
#define COL_TYPE_RGB 3
#define COL_TYPE_RGBA 4
#define PNG_DEBUG 3
#define  NMB_JPEG_EXTENSIONS 6

const char* PNG_EXTENSION = "png";
const char* JPEG_EXTENSIONS[NMB_JPEG_EXTENSIONS] = {"JPG" , "JPEG" , "JPE" , "jpeg" , "jpg" , "jpe"};

//==================================== VARIABLES ====================================//
PSeed* seeds = NULL;

FILE* lightning_intensity_file = NULL;

int width;
int height;

//========================== JPEG VARIABLES ==========================//
FILE *INFILE;
JSAMPROW ROW_POINTERS_INPUT_JPEG;

FILE *OUTFILE;
JSAMPROW ROW_POINTERS_OUTPUT_JPEG;

struct jpeg_decompress_struct cinfo;
struct jpeg_error_mgr jerr;

unsigned char *image_data;

int num_channels;

//========================== PNG VARIABLES ==========================//
int number_of_passes;

int width_output;
int height_output;
int number_of_passes_output;

png_byte color_type;
png_byte bit_depth;

png_byte color_type_output;
png_byte bit_depth_output;

png_structp PNG_PTR;
png_infop INFO_PTR;
png_bytep *ROW_POINTERS_INPUT_PNG;

png_structp PNG_PTR_OUTPUT;
png_infop INFO_PTR_OUTPUT;
png_bytep *ROW_POINTERS_OUTPUT_PNG;

//==================================== FUNCTIONS ====================================//
void abort_(const char *s, ...){
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

const char* get_file_extension(const char* filename) {

    const char* extension = strrchr(filename, '.');
    if (extension == NULL) {
        abort_("[get_file_extension] null extension");
    }
    return extension + 1;
}

bool is_jpeg(const char* extension){

    for (int i = 0 ; i < NMB_JPEG_EXTENSIONS ; i++){
        if (strcmp(extension , JPEG_EXTENSIONS[i]) == 0){
            return true;
        }
    }
    return false;
}

bool is_png(const char* extension){

    if (strcmp(extension , PNG_EXTENSION) == 0){
        return true;
    }
    else{
        return false;  
    }
}

float ** table_allocation(int width , int height){

    float** row = (float**)malloc(width * sizeof(float*));

    for (int i = 0 ; i < width ; i++){
        
        float* column = (float*)malloc(height * sizeof(float));
        for (int j = 0 ; j < height ; j++){
            column[j] = 0.f;
        }
        row[i] = column;
    }
    return row;
}

//========================== JPEG FUNCTIONS ==========================//
void read_jpeg_file(const char *input_file_name , int* width , int* height , int* num_channels){

    INFILE = fopen(input_file_name, "rb");
    if (!INFILE) {
        abort_("[read_jpeg_file] File %s could not be opened for reading", input_file_name);
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, INFILE);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    *num_channels = cinfo.output_components;

    image_data = (unsigned char *)malloc((*width) * (*height) * (*num_channels));

    while (cinfo.output_scanline < cinfo.output_height) {
        ROW_POINTERS_INPUT_JPEG = &image_data[cinfo.output_scanline * (*width) * (*num_channels)];
        jpeg_read_scanlines(&cinfo, &ROW_POINTERS_INPUT_JPEG, 1);
    }
    jpeg_finish_decompress(&cinfo);

    fclose(INFILE);
}

void write_jpeg_file(const char *output_file_name , int width , int height , int num_channels){

    OUTFILE = fopen(output_file_name, "wb");
    if (!OUTFILE) {
        abort_("[write_jpeg_file] File %s could not be opened for reading", output_file_name);
    }

    struct jpeg_compress_struct cinfo_out;
    struct jpeg_error_mgr jerr_out;

    cinfo_out.err = jpeg_std_error(&jerr_out);
    
    jpeg_create_compress(&cinfo_out);

    jpeg_stdio_dest(&cinfo_out, OUTFILE);

    cinfo_out.image_width = width;
    cinfo_out.image_height = height;
    cinfo_out.input_components = num_channels;
    cinfo_out.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo_out);
    jpeg_set_quality(&cinfo_out, 100, TRUE); 

    jpeg_start_compress(&cinfo_out, TRUE);

    while (cinfo_out.next_scanline < cinfo_out.image_height) {
        ROW_POINTERS_OUTPUT_JPEG = &image_data[cinfo_out.next_scanline * width * num_channels];
        jpeg_write_scanlines(&cinfo_out, &ROW_POINTERS_OUTPUT_JPEG , 1);
    }

    jpeg_finish_compress(&cinfo_out);

    fclose(OUTFILE);
    free(image_data);

    jpeg_destroy_compress(&cinfo_out);
    jpeg_destroy_decompress(&cinfo);
}

//========================== PNG FUNCTIONS ==========================//
void read_png_file(const char *file_name , int* width , int* height , png_byte* color_type , png_byte* bit_depth , int* number_of_passes){

    int png_size = 8;
    char header[png_size];

    FILE *fp = fopen(file_name, "rb");
    if (!fp){
        abort_("[read_png_file] File %s could not be opened for reading", file_name);
    }

    fread(header, 1, png_size, fp);
    if (png_sig_cmp(header, 0, png_size)){
        abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);
    }

    PNG_PTR = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!PNG_PTR){
        abort_("[read_png_file] png_create_read_struct failed");
    }

    INFO_PTR = png_create_info_struct(PNG_PTR);
    if (!INFO_PTR){
        abort_("[read_png_file] png_create_info_struct failed");
    }

    if (setjmp(png_jmpbuf(PNG_PTR))){
        abort_("[read_png_file] Error during init_io");
    }

    png_init_io(PNG_PTR, fp);
    png_set_sig_bytes(PNG_PTR, 8);

    png_read_info(PNG_PTR, INFO_PTR);

    *width = png_get_image_width(PNG_PTR, INFO_PTR);
    *height = png_get_image_height(PNG_PTR, INFO_PTR);
    *color_type = png_get_color_type(PNG_PTR, INFO_PTR);
    *bit_depth = png_get_bit_depth(PNG_PTR, INFO_PTR);

    *number_of_passes = png_set_interlace_handling(PNG_PTR);
    png_read_update_info(PNG_PTR, INFO_PTR);

    if (setjmp(png_jmpbuf(PNG_PTR))){
        abort_("[read_png_file] Error during read_image");
    }

    ROW_POINTERS_INPUT_PNG = (png_bytep *)malloc(sizeof(png_bytep) * (*height));
    for (int y = 0 ; y < (*height) ; y++){
        ROW_POINTERS_INPUT_PNG[y] = (png_byte *)malloc(png_get_rowbytes(PNG_PTR, INFO_PTR));
    }
    png_read_image(PNG_PTR, ROW_POINTERS_INPUT_PNG);

    fclose(fp);
}

void write_png_file(const char *file_name , int width_output , int height_output , png_byte bit_depth_output , png_byte color_type_output){

    FILE *fp = fopen(file_name, "wb");
    if (!fp){
        abort_("[write_png_file] File %s could not be opened for writing", file_name);
    }

    PNG_PTR_OUTPUT = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!PNG_PTR_OUTPUT){
        abort_("[write_png_file] png_create_write_struct failed");
    }

    INFO_PTR_OUTPUT = png_create_info_struct(PNG_PTR_OUTPUT);
    if (!INFO_PTR_OUTPUT){
        abort_("[write_png_file] png_create_info_struct failed");
    }

    if (setjmp(png_jmpbuf(PNG_PTR_OUTPUT))){
        abort_("[write_png_file] Error during init_io");
    }
    png_init_io(PNG_PTR_OUTPUT, fp);

    if (setjmp(png_jmpbuf(PNG_PTR_OUTPUT))){
        abort_("[write_png_file] Error during writing header");
    }

    png_set_IHDR(PNG_PTR_OUTPUT , INFO_PTR_OUTPUT , width_output , height_output , bit_depth_output , 
                color_type_output , PNG_INTERLACE_NONE , PNG_COMPRESSION_TYPE_BASE , PNG_FILTER_TYPE_BASE);
    png_write_info(PNG_PTR_OUTPUT, INFO_PTR_OUTPUT);

    if (setjmp(png_jmpbuf(PNG_PTR_OUTPUT))){
        abort_("[write_png_file] Error during writing bytes");
    }
    png_write_image(PNG_PTR_OUTPUT, ROW_POINTERS_OUTPUT_PNG);

    if (setjmp(png_jmpbuf(PNG_PTR_OUTPUT))){
        abort_("[write_png_file] Error during end of write");
    }
    png_write_end(PNG_PTR_OUTPUT, NULL);

    for (int y = 0 ; y < height_output ; y++){
        free(ROW_POINTERS_OUTPUT_PNG[y]);
    }
    free(ROW_POINTERS_OUTPUT_PNG);
    fclose(fp);
}

png_bytep *allocates_image_memory(int width, int height){
    png_bytep *row_pointers_output;

    row_pointers_output = (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0 ; y < height ; y++){
        row_pointers_output[y] = (png_byte *)malloc(sizeof(png_byte) * sizeof(int) * width);
    }

    return row_pointers_output;
}

//========================== VECTORS FUNCTIONS ==========================//
CVector new_CVector(){
    CVector v;

    v.x = 0.f;
    v.y = 0.f;
    v.z = 0.f;

    return v;
}

CVector update_CVector(CVector v , float x , float y , float z){

    v.x = x;
    v.y = y;
    v.z = z;

    return v;
}

float compute_distance(CVector v1 , CVector v2){
    return( ((v2.x - v1.x) * (v2.x - v1.x)) + ((v2.y - v1.y) * (v2.y - v1.y)) );
}

//========================== COLORS FUNCTIONS ==========================//
Color_RGBA get_null_color(){

    Color_RGBA color;

    color.red = 0;
    color.green = 0;
    color.blue = 0;
    color.alpha = 0;

    return color;
}

Color_RGBA get_color(int red , int green , int blue , int alpha){

    Color_RGBA color;

    color.red = red;
    color.green = green;
    color.blue = blue;
    color.alpha = alpha;

    return color;
}

//========================== SEEDS FUNCTIONS ==========================//
PSeed new_Seed(){

    PSeed s = (PSeed)malloc(sizeof(Seed));
    s -> c_vector = new_CVector();
    s -> color = get_null_color();

    return s;
}

PSeed update_Seed(CVector v , Color_RGBA color){

    PSeed s = new_Seed();
    s -> c_vector = v;
    s -> color = color;

    return s;
}

PSeed* place_Seed_JPEG(JSAMPLE *image_data , int nmb_seeds , int width , int height){

    PSeed* seeds = (PSeed*)malloc(sizeof(PSeed) * nmb_seeds);
    PSeed ptr_seed_i = NULL;
    CVector v_seed_i = new_CVector();
    Color_RGBA seed_color = get_null_color();

    int x_seed_i = 0;
    int y_seed_i = 0;

    for (int i = 0 ; i < nmb_seeds ; i++){
        
        ptr_seed_i = new_Seed();
        seed_color = get_null_color();

        x_seed_i = rand()%width;
        y_seed_i = rand()%height;
        v_seed_i = update_CVector(v_seed_i , x_seed_i , y_seed_i , 0);

        int offset = (y_seed_i * width + x_seed_i) * COL_TYPE_RGB;
        seed_color = get_color(image_data[offset] , image_data[offset+1] , image_data[offset+2] , 0);

        (ptr_seed_i -> c_vector) = v_seed_i;
        (ptr_seed_i -> color) = seed_color;

        seeds[i] = ptr_seed_i;
    }

    return seeds;
}

PSeed* place_Seed_PNG(png_bytep *ROW_POINTERS_INPUT_PNG , int nmb_seeds , int width , int height){

    PSeed* seeds = (PSeed*)malloc(sizeof(PSeed) * nmb_seeds);
    PSeed ptr_seed_i = NULL;
    CVector v_seed_i = new_CVector();
    Color_RGBA seed_color = get_null_color();

    int x_seed_i = 0;
    int y_seed_i = 0;

    png_byte* row = NULL;
    png_byte* ptr = NULL;

    for (int i = 0 ; i < nmb_seeds ; i++){
        
        ptr_seed_i = new_Seed();
        seed_color = get_null_color();

        x_seed_i = rand()%width;
        y_seed_i = rand()%height;
        v_seed_i = update_CVector(v_seed_i , x_seed_i , y_seed_i , 0);

        row = ROW_POINTERS_INPUT_PNG[y_seed_i];
        ptr = &(row[x_seed_i * 4]);
        seed_color = get_color(ptr[0] , ptr[1] , ptr[2] , ptr[3]);

        (ptr_seed_i -> c_vector) = v_seed_i;
        (ptr_seed_i -> color) = seed_color;

        seeds[i] = ptr_seed_i;
    }

    return seeds;
}

//========================== SPATIAL FREQUENCES ==========================//
float** compute_lightning_frequences(JSAMPLE *image_data , png_bytep *ROW_POINTERS_INTPUT_PNG , int width , int height){

    int red = 0;
    int green = 0;
    int blue  = 0;

    float lightning = 0.f;

    float** lightning_intensity = table_allocation(width , height);

    if(image_data != NULL && ROW_POINTERS_INTPUT_PNG == NULL){
        for (int y = 0 ; y < height ; y++){
            for (int x = 0 ; x < width ; x++){

                int offset = (y * width + x) * COL_TYPE_RGB;

                red = image_data[offset+1];
                green = image_data[offset+2];
                blue = image_data[offset+3];

                lightning = (float)(red + green + blue)/3.0;
                
                lightning_intensity[x][y] = lightning;
            }
        }
        return lightning_intensity;
    }
    else if (image_data == NULL && ROW_POINTERS_INTPUT_PNG != NULL){
        abort_("[compute_lightning_frequences] image_dat is NULL");
    }
}

void write_lightning_intensity(float** lightning_intensity , FILE* lightning_intensity_file , const char* input_file){

    if (lightning_intensity_file == NULL) {
        abort_("[write_lightning_intensity] File * is NULL");
    }

    for (int i = 0 ; i < height ; i++){
        for (int j = 0 ; j < width ; j++){
            fprintf(lightning_intensity_file , "%f ", lightning_intensity[j][i]);
        }
        fprintf(lightning_intensity_file , "\n");
    }

    fclose(lightning_intensity_file);
}

void voronizing_file(JSAMPLE *image_data_out , png_bytep *ROW_POINTERS_OUTPUT_PNG , PSeed* seeds , int nmb_seeds , int width , int height){

    float dist_min = __FLT_MAX__;
    float distance = 0;

    CVector current_pixel = new_CVector();
    PSeed pixel_seed = new_Seed();

    for (int y = 0 ; y < height ; y++){
        for (int x = 0 ; x < width ; x++){

            int offset = (y * width + x) * COL_TYPE_RGB;

            current_pixel = update_CVector(current_pixel , x , y , 0);
            pixel_seed = new_Seed();
            
            dist_min = __FLT_MAX__;
            distance = 0;

            for (int i = 0 ; i < nmb_seeds ; i++){
                distance = compute_distance((seeds[i] -> c_vector) , current_pixel);

                if (distance < dist_min){
                    dist_min = distance;
                    pixel_seed = seeds[i];
                }
            }

            if(image_data_out != NULL && ROW_POINTERS_OUTPUT_PNG == NULL){

                image_data_out[offset] = (pixel_seed -> color).red;  
                image_data_out[offset + 1] = (pixel_seed -> color).green;  
                image_data_out[offset + 2] = (pixel_seed -> color).blue; 
            }
            else if (image_data_out == NULL && ROW_POINTERS_OUTPUT_PNG != NULL){

                png_byte *row = ROW_POINTERS_OUTPUT_PNG[y];
                png_byte *ptr = &(row[x * 4]);

                ptr[0] = (pixel_seed -> color).red;
                ptr[1] = (pixel_seed -> color).green;
                ptr[2] = (pixel_seed -> color).blue;
                ptr[3] = (pixel_seed -> color).alpha;

            }
            else{
                abort_("[voronizing_file] Error during computing voronoi");
            }
        }
    }
}

//==================================== MAIN ====================================//
int main(int argc, char const *argv[]){
    
    srand(time(NULL));

    if (argc != 4){
        printf("Absents or too much args ...\n");
        printf("./voronizing [input file] [input file] [number of seeds]n\n");
        exit(1);
    }
    else{
        
        const char* input_file_name = argv[1];
        const char* output_file_name = argv[2];
        const int nmb_seeds = atoi(argv[3]);

        const char* input_file_extension = get_file_extension(input_file_name);
        const char* output_file_extension = get_file_extension(output_file_name);

        int cmp_file = strcmp(input_file_extension , output_file_extension);

        if (cmp_file != 0){
           fprintf(stderr , "The input file and the output file have not the same extension !\n");
            return EXIT_FAILURE;
        }

        bool cmp_png = is_png(input_file_extension);
        bool cmp_jpeg = is_jpeg(input_file_extension);

        if(cmp_png){
            printf("Extension : %s \nLaunching %s script ...\n\n", input_file_extension , input_file_extension);
            printf("input_file_name : %s , output_file_name : %s , nmb_seeds : %d\n\n", input_file_name , output_file_name , nmb_seeds);

            read_png_file(input_file_name , &width , &height , &color_type , &bit_depth , &number_of_passes);

            seeds = place_Seed_PNG(ROW_POINTERS_INPUT_PNG , nmb_seeds , width , height);

            width_output = width;
            height_output = height;
            color_type_output = PNG_COLOR_TYPE_RGBA;
            bit_depth_output = 8;
            number_of_passes_output = 2;

            ROW_POINTERS_OUTPUT_PNG = allocates_image_memory(width_output, height_output);

            printf("Computing voronoi ... ");
            time_t begin = time(NULL);
            voronizing_file(NULL , ROW_POINTERS_OUTPUT_PNG ,  seeds , nmb_seeds , width , height);
            time_t end = time(NULL);
            unsigned long seconds = (unsigned long) difftime(end , begin);
            printf( "Finished in %ld seconds\n\n", seconds );

            printf("Writing the voronizing picture ... ");
            write_png_file(output_file_name , width_output , height_output , bit_depth_output , color_type_output);   
            printf("DONE !\n\n");

            return EXIT_SUCCESS;
        }
        else if (cmp_jpeg){
            printf("Extension : %s \nLaunching %s script ... \n\n", input_file_extension , input_file_extension);
            printf("input_file_name : %s , output_file_name : %s , nmb_seeds : %d\n\n", input_file_name , output_file_name , nmb_seeds);

            read_jpeg_file(input_file_name , &width , &height , &num_channels);
            
            seeds = place_Seed_JPEG(image_data , nmb_seeds , width , height);

            printf("Computing voronoi ... "); 
            time_t begin = time(NULL);
            voronizing_file(image_data , NULL ,  seeds , nmb_seeds , width , height);
            time_t end = time(NULL);
            unsigned long seconds = (unsigned long) difftime(end , begin);
            printf("Finished in %ld seconds\n\n", seconds ); 

            printf("Writing the voronizing picture ...");
            write_jpeg_file(output_file_name , width , height , num_channels);
            printf("DONE !\n\n");

            return EXIT_SUCCESS;
        }
        else{
            fprintf(stderr , "Extensions not reconized\n");
            return EXIT_FAILURE;
        }
    }
}
