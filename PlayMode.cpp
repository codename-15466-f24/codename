#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "TextureProgram.hpp"
#include "Framebuffers.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/color_space.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <iostream>
#include <fstream>
#include <random>
#include <fstream>
#include <sstream>

#define FONT_SIZE 72
#define TOPMARGIN (FONT_SIZE * .5)
#define LEFTMARGIN (FONT_SIZE * 2)
#define LINE_SPACING (FONT_SIZE * 0.25)

// Storing font here, we can also refactor the text storage to load a font path
// as the first line per script or have a font buffer.

static std::string tex_path = "textures/out.png";
static std::string textbg_path = "textures/textbg.png";
//static constexpr std::string bg_path = "textures/black.png";
static std::string font_path = "fonts/RobotoMono-Regular.ttf";
static constexpr uint32_t window_height = 720;
static constexpr uint32_t window_width = 1280;
// static constexpr uint32_t render_height = window_height/3;
// static constexpr uint32_t render_width = window_width;
// static glm::u8vec4 text_render[render_height][render_width];
static std::vector<std::string> activeScript;
// static uint32_t activeIndex = 0;
// static uint32_t lastIndex = 0;
static std::vector<std::string> links;
static bool editMode = false;
static std::string editStr = "";
static std::string editStr_ui = "";
static uint32_t cursor_pos = 0;
static size_t cursor_pos_ui = 0;
static PlayMode::TextureItem* editingBox;
static bool cs_open = false;
static bool cheatsheet_open = false;
static std::string current_line = "";
static std::string correctStr = "";
static uint32_t cj = 0;
static uint32_t ij = 0;

// Leaving the cipher up here for now because the substitution is here
bool hasReversed = false;
static char substitution[26] = {'a','b','c','d','e','f','g','h','i','j','k','l','m',
                                'n','o','p','q','r','s','t','u','v','w','x','y','z'};


GLuint codename_meshes_for_lit_color_texture_program = 0;
Load<MeshBuffer> codename_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("scenes/codename.pnct"));
	codename_meshes_for_lit_color_texture_program = 
		ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load<Scene> codename_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("scenes/codename.scene"), [&](
		    Scene &scene, 
		    Scene::Transform *transform, 
			std::string const &mesh_name
	){
		Mesh const &mesh = codename_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;
		// if (mesh_name == "holo_screen") {
		// 	drawable.pipeline = texture_program_pipeline;
		// }

		drawable.pipeline.vao = codename_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > codename_bgm(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sounds/codename.opus"));
});

Load< Sound::Sample > keyclick1(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/keyclick1.opus"));
	return s;
});

Load< Sound::Sample > keyclick2(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/keyclick2.opus"));
	return s;
});

Load< Sound::Sample > door(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/door.opus"));
	return s;
});

Load< Sound::Sample > ding(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/ding.opus"));
	return s;
});

Load< Sound::Sample > chaos(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/chaos.opus"));
	return s;
});

Load< Sound::Sample > docking(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/docking.opus"));
	return s;
});

Load< Sound::Sample > chomp(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/chomp.opus"));
	return s;
});

Load< Sound::Sample > successful(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/success.opus"));
	return s;
});

Load< Sound::Sample > fail(LoadTagDefault, []() -> Sound::Sample const * {
	Sound::Sample *s = new Sound::Sample(data_path("sounds/fail.opus"));
	return s;
});

//Reset the current text (or other) png
void clear_png(PlayMode::TextureItem *png_in, uint32_t height = 0, uint32_t width = 0) {
    /*for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            png_in[y*width + x] = glm::u8vec4(0, 0, 0, 0);  // RGBA = (0, 0, 0, 0) = transparent black
        }
    }*/
	for (auto i = png_in->data.begin(); i != png_in->data.end(); ++i){
		// *i = glm::u8vec4(0,0,0,0);
	}
}

//Draw text in png format from bitmap into out. Takes a color to draw into. 
void draw_glyph_png(FT_Bitmap *bitmap, PlayMode::TextureItem *png_out, 
                    uint32_t x, uint32_t y, glm::u8vec4 color) {
	//auto it = png_out->data.begin();
	for (uint32_t i = 0; i < bitmap->rows; i++) {
        for (uint32_t j = 0; j < bitmap->width; j++) {
            uint32_t out_x = x + j;
        	uint32_t out_y = y + i;//bitmap->rows - i - 1;
            if (out_x < png_out->size.x && out_y < png_out->size.y) {
				// According to freetype.org,  buffer bytes per row is saved as bitmap->pitch
                uint8_t alpha = bitmap->buffer[i * bitmap->pitch + j];
				// Calculate the index in the RGBA buffer
                int index = (png_out->size.y-out_y-1)*png_out->size.x + out_x;
				png_out->data[index] = glm::u8vec4(color.r,color.g,color.b,alpha);
            }
        }
    }
	//std::cout << std::to_string(png_out->data.size());
}

std::string decode(std::string str_in, char key){
	std::string out = "";
	switch (key){
		case 'e':
			for (size_t i = 0; i < str_in.length(); i++){
				if (str_in[i] >= 'a' && str_in[i] <= 'z') {
					out = out + substitution[str_in[i] - 'a'];
				} else if (str_in[i] >= 'A' && str_in[i] <= 'Z') {
					char add = substitution[str_in[i] - 'A'];
					add = add - 32;
					out = out + add;
				} else {
					out = out + str_in[i];
				}
			}
			break;
		default:
			out = str_in;
			break;
	}
	//std::cout << "from " << str_in << " to " << out << std::endl;
	return out;
}

// Isn't this slightly less of a nightmare loop now?
void PlayMode::render_text(PlayMode::TextureItem *tex_in, std::string line_in, 
                           glm::u8vec4 color, char cipher = 'e', int font_size = 591283) {
	size_t choices = display_state.jumps.size();
	// links; //idk why I did this

	glm::u8vec4 colorOut = color; //Overriding it if it's a choice because I want to :D
	std::string line = "";
	if (font_size == 591283){
		font_size = tex_in->f_size;
	} else {
		tex_in->f_size = font_size;
	}

	//I wanted to get special characters done before glyphs were in so this was the easiest solution I could find
	//Numbers are unicode encodings for my keywords in the script. Change these later if we're using a different method.
	//TODO (Optional): Move newlines here too. Matias/Jim mentioned I should probably handle them before I generate glyphs
	//but not 100% on how
	
	// I removed the choice checking for now. ~Yoseph
	// I'll change it in a bit, but it doesn't work for the current script purposes right now.
	
	line = line_in;
	//std::cout << line << std::endl;
	//turn options blue
	if (choices != 1) {
		colorOut = glm::u8vec4(0,0,255,1);
	}
	
	// line = decode(line, cipher); // not ciphering for now

	bool checkSize = false;
	while (checkSize == false){

		// Based on Harfbuzz example at: 
		// https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
		// since the below code follows the code from the example basically 
		// exactly, I'm also including some annotation of my understanding of what's going on
		FT_Library ft_library;
		FT_Face ft_face;
		FT_Error ft_error;

		// Initialize Freetype basics and check for failure
		// Load freetype library into ft_library
		ft_error = FT_Init_FreeType (&ft_library);
		if (ft_error) {
			std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
			std::cout << "Init Freetype Library failed, aborting..." << std::endl;
			abort();
		}
		// Load font face through path (font_path)
		ft_error = FT_New_Face (ft_library, data_path(font_path).c_str(), 0, &ft_face);
		if (ft_error) { // .c_str() converts to char *
			std::cout << "Failed while loading " << data_path(font_path).c_str() << std::endl;
			std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
			std::cout << "Init Freetype Face failed, aborting..." << std::endl;
			abort();
		}
		// Define a character size based on constant literals
		ft_error = FT_Set_Char_Size (ft_face, font_size*64, font_size*64, 0, 0);
		if (ft_error){
			std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
			std::cout << "Setting character size failed, aborting..." << std::endl;
			abort();
		}
		
		// Initialize harfbuzz shaper using freetype font
		hb_font_t *hb_font;
		hb_font = hb_ft_font_create (ft_face, NULL); //NULL destruction function
		
		//Create and fill buffer (test text)
		hb_buffer_t *hb_buffer;
		hb_buffer = hb_buffer_create ();
		hb_buffer_add_utf8 (hb_buffer, line.c_str(), -1, 0, -1); // -1 length values (buffer, string) with 0 offset
		hb_buffer_guess_segment_properties (hb_buffer);

		hb_shape (hb_font, hb_buffer, NULL, 0); // this actually defining the gylphs into hb_buffer
		
		// extract more info about the glyphs
		unsigned int len = hb_buffer_get_length (hb_buffer);
		hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
		hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);
		

		// Following is derived from the Freetype example at: 
		// https://freetype.org/freetype2/docs/tutorial/step1.html
		FT_GlyphSlot  slot = ft_face->glyph; // 
		int           pen_x, pen_y;
		// static uint32_t h = window_height/3;
		//reset png
		clear_png(tex_in);
		//ensure it's correct size
		std::vector<glm::u8vec4> cleared_data(tex_in->size.x * tex_in->size.y, glm::u8vec4(0,0,0,0));
		tex_in->data = cleared_data;
		//std::cout << "Size: " << std::to_string(tex_in->data.size()) << std::endl;
		//std::cout << "First Item: " << glm::to_string(tex_in->data[0]) << std::endl;

		//pen_x = static_cast<int>(LEFTMARGIN);
		//pen_y = static_cast<int>(TOPMARGIN) + font_size;
		pen_x = tex_in->margin.x;
		pen_y = tex_in->margin.y + font_size;

		double line_height = font_size;
		// bool lastWasSpace = true;
		bool lastWasNewLine = true;
		std::string glyphname = "";
		double final_y = 0.;
		char gn[32] = "";
		uint32_t endofline = 0;
		for (uint32_t n = 0; n < len; n++ ) {
			memset(gn, 0, sizeof(gn));
			hb_codepoint_t gid   = info[n].codepoint;
			hb_font_get_glyph_name (hb_font, gid, gn, sizeof(gn)/sizeof(char));
			glyphname.assign(gn, strlen(gn));
			// std::cout << gn << std::endl;
			// std::cout << glyphname;
			// unsigned int cluster = info[n].cluster;
			double x_position = pen_x + pos[n].x_offset / 64.;
			double y_position = pen_y + pos[n].y_offset / 64.;

			if (glyphname == "franc") {
				//New Line
				pen_x = tex_in->margin.x; 
				pen_y += static_cast<int>(line_height + LINE_SPACING); 
				x_position = pen_x + pos[n].x_offset / 64.;
				y_position = pen_y + pos[n].y_offset / 64.;
				final_y = y_position;

				line_height = font_size;
				lastWasNewLine = true;
			}
			if (n == endofline && n != 0){
				pen_x = tex_in->margin.x; 
				pen_y += static_cast<int>(line_height + LINE_SPACING); 
				x_position = pen_x + pos[n].x_offset / 64.;
				y_position = pen_y + pos[n].y_offset / 64.;
				final_y = y_position;
				
				line_height = font_size;
				lastWasNewLine = true;
			}

			// Cumbersome but this lets us automatically handle new lines.
			float wordWidth = 0.0f;
			std::string word = "";
			std::string loop_glyphname = "";
			if (lastWasNewLine) {
				float lineWidth = 0.0f;
				std::string loop_line_glyphname = "";
				bool keepLooping = true;
				//std::cout << "checking line" << std::endl;
				uint32_t m = n;
				uint32_t lastGoodM = 0;
				// Had to make the condition a bool here; originally cased on 
				// lineWidth but I want to store the maximum lineWidth below the threshold
				while (keepLooping == true && m < len){
					wordWidth = 0.0f;
					//std::string thisWord = "";
					loop_line_glyphname = "";
					//Compute next word
					while (loop_line_glyphname != "space" && loop_line_glyphname != "franc" && m < len) {
						hb_codepoint_t loop_gid  = info[m].codepoint;
						char llgn_array[32] = "";
						memset(llgn_array, 0, sizeof(llgn_array));
						hb_font_get_glyph_name (hb_font, loop_gid, llgn_array, sizeof (llgn_array));
						loop_line_glyphname.assign(llgn_array, strlen(llgn_array));
						//thisWord = thisWord + loop_line_glyphname;
						wordWidth += pos[m].x_advance / 64;
						m+=1;
					}
					//std::cout << thisWord << std::endl;
					//Check if current line + word is larger than final line
					if (lineWidth + wordWidth < tex_in->size.x - tex_in->margin.x){
						lineWidth += wordWidth;
						lastGoodM = m;
						//std::cout << "current width = " << std::to_string(lineWidth) << std::endl;
					} else {
						//std::cout << "Width in loop " << std::to_string(lineWidth) << std::endl;
						keepLooping = false;
					}
					//EOL
					if (loop_line_glyphname == "franc" ){
						keepLooping = false;
						break;
					}
				}
				//std::cout << "This line is " << std::to_string(lineWidth) << "px wide against width " << std::to_string(tex_in->size.x) << std::endl;
				switch (tex_in->align){
					case (MIDDLE):
						pen_x = int((float(tex_in->size.x) - lineWidth)/2.0f);
						break;
					case (RIGHT):
						pen_x = (tex_in->size.x) - int(lineWidth) + (tex_in->margin.x);
						break;
					default:
						pen_x = tex_in->margin.x;
						break;
				}
				endofline = lastGoodM;
				lastWasNewLine = false;
				x_position = pen_x + pos[n].x_offset / 64.; 
			}
			/*
			if (lastWasSpace) {
				uint32_t m = n;
				while (loop_glyphname != "space" && loop_glyphname != "uni20BF" && m < len){
					hb_codepoint_t loop_gid  = info[m].codepoint;
					char lgn_array[32] = "";
					memset(lgn_array, 0, sizeof(lgn_array));
					hb_font_get_glyph_name (hb_font, loop_gid, lgn_array, sizeof (lgn_array));
					loop_glyphname.assign(lgn_array, strlen(lgn_array));
					word = word + loop_glyphname;
					wordWidth += pos[m].x_advance / 64;
					m+=1;
				}

				if (x_position + wordWidth >= tex_in->size.x - tex_in->margin.x) {
					//New Line
					pen_x = tex_in->margin.x; 
					pen_y += static_cast<int>(line_height + LINE_SPACING); 
					x_position = pen_x + pos[n].x_offset / 64.;
					y_position = pen_y + pos[n].y_offset / 64.;
					final_y = y_position;

					line_height = font_size;
					lastWasNewLine = true;
				}
			}*/

			// The above solution to lines does not allow for \n since it's based in glyphs.
			// Instead of \n,  I will be using ₿ to indicate line ends.
			// I've adjusted my text asset pipeline to account for this.
			// NOTE: This is probably why Matias/Jim told me to process it before glyphifying, lol
			
			// load glyph image into the slot (erase previous one) 
			FT_Error error = FT_Load_Glyph(ft_face, gid, FT_LOAD_RENDER); // Glyphs instead of Chars
			if (error) continue; 

			// track max line_height for unification
			if (slot->bitmap.rows > line_height) {
				line_height = slot->bitmap.rows;
			}
			
			// bitmap drawing function
			if (glyphname != "franc") 
			{
				if (colorOut == green && ((cs_open && n == cursor_pos) || 
					(cheatsheet_open && n == cursor_pos_ui))){
					draw_glyph_png(&slot->bitmap, tex_in, static_cast<int>(x_position + slot->bitmap_left), static_cast<int>(y_position - slot->bitmap_top), white);
				}
				else {
					draw_glyph_png(&slot->bitmap, tex_in, static_cast<int>(x_position + slot->bitmap_left), static_cast<int>(y_position - slot->bitmap_top), colorOut);
				}
				// Move the "pen" based on x and y advance given by glyphs
				pen_x += pos[n].x_advance / 64; 
				pen_y += pos[n].y_advance / 64; 

			} else {
				continue;
			}

			// if (glyphname == "space"){
			// 	lastWasSpace = true;
			// } else {
			// 	lastWasSpace = false;
			// }
		}
		if (final_y + line_height > tex_in->size.y){
			if (font_size == tex_in->f_size){
				tex_in->f_size -= 2;
				font_size = tex_in->f_size;
			}
		} else {
			checkSize = true;
		}
		hb_buffer_destroy (hb_buffer);
		hb_font_destroy (hb_font);

		FT_Done_Face (ft_face);
		FT_Done_FreeType (ft_library);
	}
}

//x0,x1,y0,y1,z encode the coords for where on the screen the texture is.
int update_texture(PlayMode::TextureItem *tex_in){
	// Code is from Jim via Sasha's notes, cross-referenced against Jim's copied
	// code in discord where Sasha's notes were missing, with a few changes to make this
	// work in this context

	glGenTextures(1, &tex_in->tex);
	{ // upload a texture
		// note: load_png throws on failure
		//glm::uvec2 size;
		//std::vector<glm::u8vec4> data;
		if (tex_in->loadme == true){
			load_png(data_path(tex_in->path), &tex_in->size, &tex_in->data, LowerLeftOrigin);
		}
		for (size_t i = 0; i < tex_in->data.size(); i++) {
				tex_in->data[i] = glm::u8vec4(255.f * glm::convertSRGBToLinear(glm::vec4(tex_in->data[i]) / 255.f));
		}
		glBindTexture(GL_TEXTURE_2D, tex_in->tex);
		// here, "data()" is the member function that gives a pointer to the first element
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_in->size.x, tex_in->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_in->data.data());

		glGenerateMipmap(GL_TEXTURE_2D); // MIPMAP!!!!!! realizing this was missing took a while...
		// i for integer. wrap mode, minification, magnification
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// nearest neighbor
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// aliasing at far distances. maybe moire patterns
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	GL_ERRORS();
	glGenBuffers(1, &tex_in->tristrip);
	
	glGenVertexArrays(1, &tex_in->tristrip_for_texture_program);
	{ //set up vertex array:
		glBindVertexArray(tex_in->tristrip_for_texture_program);
		glBindBuffer(GL_ARRAY_BUFFER, tex_in->tristrip);

		glVertexAttribPointer( texture_program->Position_vec4, 3, GL_FLOAT, GL_FALSE, sizeof(PosTexVertex), (GLbyte *)0 + offsetof(PosTexVertex, Position) );
		glEnableVertexAttribArray( texture_program->Position_vec4 );

		glVertexAttribPointer( texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(PosTexVertex), (GLbyte *)0 + offsetof(PosTexVertex, TexCoord) );
		glEnableVertexAttribArray( texture_program->TexCoord_vec2 );

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	GL_ERRORS();

	std::vector< PosTexVertex > verts;
	
	// Define top right quadrant to full texture
	verts.emplace_back(PosTexVertex{
		.Position = glm::vec3(tex_in->bounds[0], tex_in->bounds[2], tex_in->bounds[4]),
		.TexCoord = glm::vec2(0.0f, 0.0f), // extra comma lets you make a 1 line change later
	});
	verts.emplace_back(PosTexVertex{
		.Position = glm::vec3(tex_in->bounds[0], tex_in->bounds[3], tex_in->bounds[4]),
		.TexCoord = glm::vec2(0.0f, 1.0f),
	});
	verts.emplace_back(PosTexVertex{
		.Position = glm::vec3(tex_in->bounds[1], tex_in->bounds[2], tex_in->bounds[4]),
		.TexCoord = glm::vec2(1.0f, 0.0f),
	});
	verts.emplace_back(PosTexVertex{
		.Position = glm::vec3(tex_in->bounds[1], tex_in->bounds[3], tex_in->bounds[4]),
		.TexCoord = glm::vec2(1.0f, 1.0f),
	});
	/*for (int i = 0; i < verts.size(); i++) {
		std::cout << glm::to_string(verts[i].Position) << std::endl;
		std::cout << glm::to_string(verts[i].TexCoord) << std::endl;
	}*/
	glBindBuffer(GL_ARRAY_BUFFER, tex_in->tristrip);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(verts[0]), 
	             verts.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	tex_in->count = GLsizei(verts.size());
	GL_ERRORS();

	
	//identity transform (just drawing "on the screen"):
	tex_in->CLIP_FROM_LOCAL = glm::mat4(1.0f);
	
	GL_ERRORS();
	return 0;
}

void PlayMode::initializeCallbacks()
{

	for (std::string path : paths)
	{
		std::cout << path << std:: endl;
		if (path == "textures/special_request_collapsed.png")
		{
			// icon that opens special request menu
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, LeftPane);
				tex_special_ptr->visible = true;
			};

			callbacks.emplace_back(callback);
		}
		else if (path == "textures/special_request.png")
		{
			// special request menu
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, LeftPane);
				tex_special_ptr->visible = false;
			};

			callbacks.emplace_back(callback);
		} else if (path == "textures/inventory_collapsed.png")
		{
			// icon that opens special request menu
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, LeftPaneMiddle);
			};

			callbacks.emplace_back(callback);
		}
		else if (path == "textures/inventory.png")
		{
			// special request menu
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, LeftPaneMiddle);
			};

			callbacks.emplace_back(callback);
		} 
		else if (path == "textures/cipher_panel.png")
		{
			// cipher panel button, on click expands the cipher panel
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				if (display_state.special_cipher->name == "Substitution"
				 || display_state.special_cipher->name == "Shaper"
				 || display_state.special_cipher->name == "CSMajor")
				{
					if (display_state.solved_puzzle)
					{
						tex_rev_ptr->visible = true;
					}
					

					if (!cs_open && !(display_state.status == INPUT))
					{
						cheatsheet_open = true;
						editingBox = tex_rev_ptr;
						editStr_ui = std::string(substitution_display);
						std::cout << substitution_display << std::endl;
						std::cout << editStr_ui << std::endl;
						cursor_pos_ui = 0;				
						editMode = true;
						clear_png(tex_rev_ptr);
						render_text(tex_rev_ptr, "abcdefghijklmnopqrstuvwxyz₣" + editStr_ui, green, 'd', 48);
						update_texture(tex_rev_ptr);
					} else {
						clear_png(tex_rev_ptr);
						render_text(tex_rev_ptr, "abcdefghijklmnopqrstuvwxyz₣" + std::string(substitution_display), white, 'd', 48);
						update_texture(tex_rev_ptr);
					}

					

				} else if (display_state.special_cipher->name == "Bleebus"
					    || display_state.special_cipher->name == "Reverse")
				{
					// reverse cipher here
					if (display_state.solved_puzzle)
					{
						tex_rev_ptr->visible = true;
					}
				} else {
					// no cipher, do nothing (probably?)
				}
				togglePanel(textures, RightPane);
			};

			callbacks.emplace_back(callback);
		}
		else if (path == "textures/cipher_panel_full.png")
		{
			// full cipher panel, on click collapses the cipher panel
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				
				togglePanel(textures, RightPane);
				if (display_state.special_cipher->name == "Substitution"
				 || display_state.special_cipher->name == "Shaper"
				 || display_state.special_cipher->name == "CSMajor")
				{

					if (cheatsheet_open)
					{
						editMode = false;
						editStr_ui = "";
						cursor_pos = 0;
						cursor_pos_ui = 0;
						cheatsheet_open = false;
					}

					tex_rev_ptr->visible = false;
					
				} else if (display_state.special_cipher->name == "Bleebus"
					    || display_state.special_cipher->name == "Reverse")
				{
					// reverse cipher here
					if (display_state.solved_puzzle)
					{
						tex_rev_ptr->visible = false;
					}
				} else {
					// no cipher, do nothing (probably?)
				}
			};

			callbacks.emplace_back(callback);
		} else if (path.substr(9,8) == "customer")
		{

			// special customer selector, either select or deselect customer
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				for (auto tex : textures)
				{
					if (tex->alignment == MiddlePane ||
						tex->alignment == MiddlePaneBG ||
						tex->alignment == MiddlePaneSelected	
						)
					{
						tex->visible = false;
						tex_minipuzzle_ptr->visible = false;
						cs_open = false;
					}
				}

				// We detect the substring "selected" rather than
				// the path length, to accommodate more than 9
				// potential customers and also 'cause I wanna use customer
				// names rather than numbers. I don't want to map numbers to 
				// names to GameCharacter structs to asset indices.
				
				std::string cname;
				// get customer name
				cname = path.substr(18, path.length() - 22);
				std::cout << "selecting customer: " << cname << std::endl;
				std::unordered_map<std::string, GameCharacter>::iterator g_pair = characters.find(cname);
				if (g_pair == characters.end()) {
					std::cout << "Selected character has not been introduced yet: " 
								<< cname << std::endl;
					return;
				}

				// deselect currently selected customer			
				GameCharacter *g = &(g_pair->second);
				if (selected_character && !(selected_character->character_completed)) {
					printf("selected_character variable: %s\n", 
							selected_character->id.c_str());
					getTexture(textures, "textures/customer_" + selected_character->id  + "_selected.png")->visible = false;
					getTexture(textures, "textures/customer_" + selected_character->id  + ".png")->visible = true;
					leave_line(selected_character);
				} else {
					printf("selected_character is null\n");
				} 
			
				if (selected_character != g) {
					getTexture(textures, "textures/customer_" + cname + "_selected.png")->visible = true;
					getTexture(textures, "textures/customer_" + cname + ".png")->visible = false;
					join_line(g);
					std::string chfilecommand = "-1 Change_File ";
					apply_command(chfilecommand.append(g->entrance_file));
					while (display_state.status == CHANGING) advance_one_line(0);
					draw_state_text();
				}
			};

			callbacks.emplace_back(callback);
		} else if (path == "textures/reverse_button.png")
		{
			// button that reverses the text
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				std::cout << "pressed!" << std::endl;
				// toggle selected version on
				for (auto tex : textures)
				{
					if (tex->path == "textures/reverse_button.png")
					{
						tex->visible = false;
					}

					if (tex->path == "textures/reverse_button_selected.png")
					{
						tex->visible = true;
					}
				}
				CipherFeature cf;
				cf.b = true;
				display_state.progress_cipher->set_feature("flip", cf);
				display_state.puzzle_text = display_state.progress_cipher->encode(
					display_state.special_cipher->encode(
					display_state.solution_text));
				draw_state_text();
			};
			callbacks.emplace_back(callback);


		} else if (path == "textures/reverse_button_selected.png")
		{
			// button that unreverses the text
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				// toggle unselected version on
				for (auto tex : textures)
				{
					if (tex->path == "textures/reverse_button_selected.png")
					{
						tex->visible = false;
					}

					if (tex->path == "textures/reverse_button.png")
					{
						tex->visible = true;
					}
				}
				CipherFeature cf;
				cf.b = false;
				display_state.progress_cipher->set_feature("flip", cf);
				display_state.puzzle_text = display_state.progress_cipher->encode(
					display_state.special_cipher->encode(
					display_state.solution_text));
				draw_state_text();
			};

			callbacks.emplace_back(callback);
		}
		else if (path == "textures/submitbutton.png")
		{
			// submit button for mini puzzle window
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				bool solved = false;

				if (display_state.special_cipher->name == "Substitution"
					|| display_state.special_cipher->name == "Shaper"
					|| display_state.special_cipher->name == "CSMajor")
				{


					// TODO: Actually add-in solve checking
					std::cout << display_state.solution_text << " versus " << editStr << std::endl;
					solved = display_state.solution_text == editStr;
					std::cout << solved << std::endl;

					if (solved)
					{
						// decode first
						// propagate the answer from the minipuzzle to the key
						std::cout << display_state.puzzle_text << ", " << editStr << std::endl;
						curr_sound.emplace_back(Sound::play(*successful, 0.3f, 0.0f));
						for (size_t i = 0; i < display_state.puzzle_text.length(); i++)
						{
							size_t index = display_state.puzzle_text[i] - 'A';
							if (0 <= index && index < 26)
							{
								substitution_display[index] = (char)tolower(editStr[i]);
								// if ('A' <= editStr[i] && editStr[i] <= 'Z')
									display_state.progress_cipher
									->features["substitution"].alphabet[index] = (char)tolower(editStr[i]);
							}

						}

						for (size_t i = 0; i < 26; i++) {
							std::cout << char('A' + i) << " -> " << display_state.progress_cipher
									->features["substitution"].alphabet[i] << std::endl;
						}

						tex_minipuzzle_ptr->visible = false;
						display_state.solved_puzzle = true;
						advance_state(0);
						display_state.special_request_text = display_state.progress_cipher->encode(
							display_state.special_cipher->encode(
							display_state.special_original_text));
						std::cout << display_state.special_request_text << std::endl;
						draw_state_text();

						if (cs_open) {
							cs_open = false;
						}
						editMode = false;

						editStr = "";
						cursor_pos = 0;
						cursor_pos_ui = 0;
						display_state.status = CHANGING;

						togglePanel(textures, RightPane);
						tex_rev_ptr->visible = true;

						for (auto tex : textures)
						{
							if (tex->alignment == MiddlePane || 
								tex->alignment == MiddlePaneBG || 
								tex->alignment == MiddlePaneSelected)
							{
								tex->visible = false;
							}
						}
					}
					
				} else if (display_state.special_cipher->name == "Bleebus"
					    || display_state.special_cipher->name == "Reverse")
				{
					// example: check if reverse button is enabled
					bool reverseEnabled = true;

					for (auto tex : textures)
					{
						// only member of middlepaneselected is the reverse button
						// for the submit button for other mini puzzles, can check
						// other things before submitting
						if (tex->alignment == MiddlePaneSelected &&
							!tex->visible)
						{
							reverseEnabled = false;
							break;
						}

						if (tex->alignment == MiddlePane || 
							tex->alignment == MiddlePaneBG || 
							tex->alignment == MiddlePaneSelected)
						{
							tex->visible = false;
						}
					}

					solved = reverseEnabled;

					// keep the window up if reverse isn't enabled yet
					if (!reverseEnabled)
					{
						for (auto tex : textures)
						{
							if (tex->alignment == MiddlePane || 
								tex->alignment == MiddlePaneBG)
							{
								tex->visible = true;
							}
						}

						std::cout << "Cipher incorrect" << std::endl;

					} else {
						std::cout << "Submitted" << std::endl;
						hasReversed = true;
						tex_minipuzzle_ptr->visible = false;
						display_state.solved_puzzle = true;
						advance_state(0);

						for (auto tex : textures)
						{
							if (tex->path == "textures/cipher_panel_full.png" && !tex->visible)
							{
								togglePanel(textures, RightPane);
								tex_rev_ptr->visible = true;
							}
						}

						// Propagate the cipher text. This is going to get unwieldy eventually.
						// First change the necessary features.
						
						// do not flip anymore
						CipherFeature cf;
						cf.b = true;
						display_state.progress_cipher->set_feature("flip", cf);

						// Actually propagate the special request text
						display_state.special_request_text = display_state.progress_cipher->encode(
							display_state.special_cipher->encode(
							display_state.special_original_text));
						draw_state_text();
						// tex_special_ptr->visible = false;
				}
			}

			if (!solved)
			{
				counter = counter > 0 ? 0 : counter + 1;
				std::vector<std::string> responses = {"That doesn't seem right...", "The customer seems unsatisfied by that."};
				curr_sound.emplace_back(Sound::play(*fail, 0.3f, 0.0f));
				render_text(&tex_box_text, responses[counter], white, display_state.cipher);
				update_texture(&tex_box_text);
			}
		};
			callbacks.emplace_back(callback);
		}
		else
		{
			// default case, do nothing for the callback
			callbacks.emplace_back([&](std::vector<TexStruct *> textures, std::string path){});
		}
	}
}

void PlayMode::join_line(PlayMode::GameCharacter *g) {
	if (selected_character != nullptr)
	{
		leave_line(selected_character);
	}
	selected_character = g;
	g->joining_line = g->leaving_line == 1 ? 2 : 1;
}

void PlayMode::leave_line(PlayMode::GameCharacter *g) {
	g->leaving_line = g->joining_line == 1 ? 2 : 1;
	if (selected_character == g) selected_character = nullptr;
}

void PlayMode::clean_curr(){
	std::vector<std::shared_ptr< Sound::PlayingSample >> copy;
	for (int i = 0; i < PlayMode::curr_sound.size(); i++){
		if (!PlayMode::curr_sound[i]->stopped){
			copy.emplace_back(PlayMode::curr_sound[i]);
		}
	}
	//std::cout << "Cleaned: " << std::to_string(curr_sound.size() - copy.size()) << " sounds" << std::endl;
	PlayMode::curr_sound = copy;
}

PlayMode::PlayMode() : scene(*codename_scene) {
	tex_special_ptr = &tex_special;
	tex_minipuzzle_ptr = &tex_minipuzzle;
	tex_rev_ptr = &tex_rev;
	tex_cs_ptr = &tex_cs;
	substitution_display_ptr = &substitution_display;

	//get pointers to character transforms
	for (auto &transform : scene.transforms) {
		if (transform.name == "swap_creature") sp_shaper  = &transform;
		if (transform.name == "Bleebus_Head")  basicbleeb = &transform;
		if (transform.name == "Bleebus2")      subeelb    = &transform;
		if (transform.name == "CSMajor_Body")  gremlin    = &transform;
		if (transform.name == "CSMajor2")      csm1       = &transform;
		if (transform.name == "CSMajor3")      csm2       = &transform;
		if (transform.name == "shaper2")       g1_shaper  = &transform;
		if (transform.name == "shaper3")       g2_shaper  = &transform;
		if (transform.name == "shaper4")       g3_shaper  = &transform;
	}
	// check that all characters' transforms are founc in the scene
	{
		if (basicbleeb == nullptr) throw std::runtime_error("basicbleeb not found.");
		if (subeelb    == nullptr) throw std::runtime_error("subeelb not found.");
		if (gremlin    == nullptr) throw std::runtime_error("gremlin not found.");
		if (csm1       == nullptr) throw std::runtime_error("csm1 not found.");
		if (csm2       == nullptr) throw std::runtime_error("csm2 not found.");
		if (sp_shaper  == nullptr) throw std::runtime_error("sp_shaper not found.");
		if (g1_shaper  == nullptr) throw std::runtime_error("g1_shaper not found.");
		if (g2_shaper  == nullptr) throw std::runtime_error("g2_shaper not found.");
		if (g3_shaper  == nullptr) throw std::runtime_error("g3_shaper not found.");
	}
	creature_xforms = {basicbleeb, subeelb, 
	                   gremlin, csm1, csm2, 
	                   sp_shaper, g1_shaper, g2_shaper, g3_shaper};

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
  
	// read from the file
	display_state.current_lines = lines_from_file(display_state.file);
	display_state.jumps.push_back(1);
	editingBox = &tex_box_text;

	initializeCallbacks();
	textures = initializeTextures(alignments, visibilities, callbacks);
	addTextures(textures, paths, texture_program);

	//Add BGM
	curr_sound.emplace_back(Sound::loop(*codename_bgm, 0.2f, 0.0f));

	for (uint8_t i = 0; i < colorscheme.size() - 2; i+=3) {
		glm::vec3 new_col = glm::convertSRGBToLinear(glm::vec3(colorscheme[i], colorscheme[i+1], colorscheme[i+2]));
		colorscheme[i] = new_col.x;
		colorscheme[i+1] = new_col.y;
		colorscheme[i+2] = new_col.z;
	}

	entrance_filenames["basicbleeb"] = "basic_bleeb1.txt";
	entrance_filenames["subeelb"]    = "special_bleeb_call1.txt";
	entrance_filenames["csm1"]       = "cs_major_1.txt";
	entrance_filenames["csm2"]       = "cs_major_2.txt";
	entrance_filenames["gremlin"]    = "cs_major_special.txt";
	entrance_filenames["sp_shaper"]  = "shaper_special.txt";
	entrance_filenames["g1_shaper"]  = "shaper_1.txt";
	entrance_filenames["g2_shaper"]  = "shaper_2.txt";
	entrance_filenames["g3_shaper"]  = "shaper_3.txt";

	advance_state(0);
}

void PlayMode::advance_one_line(uint32_t jump_choice) {
	uint32_t place = display_state.jumps[jump_choice] - 1;
	update_one_line(place);
}

// Advance the script by one line.
void PlayMode::update_one_line(uint32_t location) {
	apply_command(display_state.current_lines[location]);
}

// Apply any script line.
void PlayMode::apply_command(std::string line) {
	std::cout << line << std::endl;
	std::vector<std::string> parsed = parse_script_line(line, " ");

	// Note: A command with a negative number has its line number ignored.
	// This is useful for applying custom lines
	int next_num = atoi(parsed[0].c_str());
	if (next_num >= 0) display_state.line_number = atoi(parsed[0].c_str());

	std::string keyword = parsed[1];
	if (keyword == "Character") {
		if (characters.find(parsed[2]) == characters.end()) {
			GameCharacter g;
			g.id = parsed[2];
			g.name = parsed[3];
			g.entrance_file = entrance_filenames[g.id];

			if (parsed[4] == "Bleebus") {
				// USE THIS ONE
				g.species = new ReverseCipher("Bleebus");
				// testing protocols for other ciphers so far:
				// g.species = new CaesarCipher("CSMajor", 5);
				// g.species = new SubstitutionCipher("Shaper", "cabdefghijklmnopqrstuvwxyz");
				// getTexture(textures, "textures/reverse_button.png")->alignment = MiddlePaneHidden;
				// getTexture(textures, "textures/reverse_button_selected.png")->alignment = MiddlePaneHidden;
			}
			else if (parsed[4] == "CSMajor" || parsed[4] == "CS-Major") {
				// since we're doing this as a substitution cipher
				// g.species = new SubstitutionCipher("CSMajor", "fghijklmnopqrstuvwxyzabcde");
				g.species = new SubstitutionCipher("CSMajor", "zabcdefghijklmnopqrstuvwxy");
			}
			else if (parsed[4] == "Shaper") {
				// probably change this to something more elaborate
				g.species = new SubstitutionCipher("Shaper", "xyuvsjifghrqpnelmktoacwdbz");
			}
			else {
				g.species = new ToggleCipher();
			}

			if (g.id == "basicbleeb")     g.asset_idx = 0;
			else if (g.id == "subeelb")   g.asset_idx = 1;
			else if (g.id == "gremlin")   g.asset_idx = 2;
			else if (g.id == "csm1")      g.asset_idx = 3;
			else if (g.id == "csm2")      g.asset_idx = 4;
			else if (g.id == "sp_shaper") g.asset_idx = 5;
			else if (g.id == "g1_shaper") g.asset_idx = 6;
			else if (g.id == "g2_shaper") g.asset_idx = 7;
			else if (g.id == "g3_shaper") g.asset_idx = 8;
			else { // we wanna assign the player this index probably
				g.asset_idx = -1;
			}

			if (g.id != "player")
			{
				join_line(&g);
			}

			characters[g.id] = g;
		}
		else {
			std::cerr << display_state.file + " " + parsed[0] + ": Found a character with this ID already. No action taken" << std::endl;
		}
		display_state.status = CHANGING;
	}
	else if (keyword == "Exit") {
		if (characters.find(parsed[2]) == characters.end()) {
			printf("Error in 'Exit' script command: Character %s not found\n", parsed[2].c_str());
			display_state.jumps = {display_state.line_number + 1};
			display_state.status = CHANGING;
			return;
		}
		std::unordered_map<std::string, GameCharacter>::iterator g_pair = characters.find(parsed[2]);

		if (selected_character != nullptr && selected_character->id == (g_pair->second).id)
		{
			prev_character = "";
		}

		if (g_pair != characters.end()) {
			leave_line(&(g_pair->second));
		}

		getTexture(textures, "textures/customer_" + (g_pair->second).id +  "_selected.png")->visible = false;
		getTexture(textures, "textures/customer_" + (g_pair->second).id + ".png")->visible = false;
		getTexture(textures, "textures/customer_" + (g_pair->second).id + "_selected.png")->alignment = TopMiddlePaneHidden;
		getTexture(textures, "textures/customer_" + (g_pair->second).id + ".png")->alignment = TopMiddlePaneHidden;

		(g_pair->second).character_completed = true;
		display_state.status = CHANGING;
	}
	else if (keyword == "Kick") {
		if (characters.find(parsed[2]) == characters.end()) {
			printf("Error in 'Kick' script command: Character %s not found\n", parsed[2].c_str());
			display_state.jumps = {display_state.line_number + 1};
			display_state.status = CHANGING;
			return;
		}
		std::unordered_map<std::string, GameCharacter>::iterator g_pair = characters.find(parsed[2]);
		prev_character = g_pair->first;

		if (g_pair != characters.end()) {
			leave_line(&(g_pair->second));
		}
		
		getTexture(textures, "textures/customer_" + (g_pair->second).id +  "_selected.png")->visible = false;
		getTexture(textures, "textures/customer_" + (g_pair->second).id + ".png")->visible = true;
		display_state.status = CHANGING;
	}
	else if (keyword == "Display") {
		if (characters.find(parsed[2]) != characters.end()) {
			// loop through existing characters on display, which should be fine since there aren't many
			for (auto it = display_state.chars.begin(); it != display_state.chars.end(); ++it) {
				if (it->ref->id == parsed[2]) display_state.chars.erase(it);
			}
			DisplayCharacter dc;
			dc.ref = &characters[parsed[2]];
			dc.pos = MIDDLE; // TODO: change depending on code
			display_state.chars.push_back(dc);
		}
		else {
			std::cerr << display_state.file + " " + parsed[0] + ": There was no character with ID " + parsed[2] << std::endl;
		}
		display_state.status = CHANGING;
	}
	else if (keyword == "Remove") {
		if (characters.find(parsed[2]) != characters.end()) {
			// loop through existing characters on display, which should be fine since there aren't many
			for (auto it = display_state.chars.begin(); it != display_state.chars.end(); ++it) {
				if (it->ref->id == parsed[2]) display_state.chars.erase(it);
			}
		}
		display_state.status = CHANGING;
	}
	else if (keyword == "Clear") {
		display_state.chars.clear();
		display_state.images.clear();
		display_state.status = CHANGING;
	}
	else if (keyword == "Speech") {
		std::string speech_text = parsed[3];
		bool found_character = false;
		GameCharacter speaker;

		display_state.cipher = 'd';
		if (parsed[2] == player_id) {
			display_state.bottom_text = parsed[3];
			found_character = true;
			speaker = characters[parsed[2]];
		}
		else if (characters.find(parsed[2]) != characters.end()) {
			found_character = true;
			speaker = characters[parsed[2]];
			std::cout << speaker.species->name << std::endl;
			// std::string res = speaker.species->encode(parsed[3]);
			std::string res = parsed[3];
			std::cout << res << std::endl;
			speech_text = res;
			display_state.cipher = 'e';
		}

		display_state.bottom_text = "";
		display_state.speech_text = "";

		display_state.bottom_text = (found_character ? "[" + speaker.name + "]₿" : "") + speech_text;
		display_state.status = TEXT;
	}
	else if (keyword == "Text") {
		display_state.cipher = parsed[2][0];
		display_state.bottom_text = parsed[3];
		display_state.status = TEXT;
	}
	else if (keyword == "Input") {
		display_state.cipher = parsed[2][0];
		display_state.bottom_text = parsed[3];
		// something similar but with text input like we discussed
		editMode = true;
		display_state.status = INPUT;
		editingBox = &tex_box_text;
	}
	else if (keyword == "Image") {
		// TODO
		display_state.status = IMAGE;
	}
	else if (keyword == "Change_File") {
		display_state.file = parsed[2];
		display_state.current_lines = lines_from_file(display_state.file);
		display_state.bottom_text = "";
		display_state.line_number = 0;
		display_state.jumps.clear();
		display_state.jumps.push_back(1);
		display_state.status = CHANGING;
	} 
	else if (keyword == "Show") {
		auto panel = parsed[2];
		if (panel == "mini_puzzle")
		{
			getTexture(textures, "textures/cipher_panel_full.png")->visible = false;
			getTexture(textures, "textures/cipher_panel.png")->visible = true;
			cheatsheet_open = false;
			tex_rev.visible = false;

			display_state.solution_text = parsed[4];
			display_state.special_cipher = characters[parsed[3]].species;

			if (display_state.progress_cipher->cipher_type() != display_state.special_cipher->cipher_type()) {
				std::cout << "reset progress" << std::endl;
				display_state.progress_cipher = new ToggleCipher();
				if (display_state.special_cipher->name == "Bleebus"
					|| display_state.special_cipher->name == "Reverse") {
					display_state.progress_cipher = new ReverseCipher();
				}
				else {
					display_state.progress_cipher = new SubstitutionCipher();
				}
			}
			std::cout << "Cipher in use for this puzzle: " << display_state.special_cipher->name << std::endl;
			// display_state.special_cipher->reset_features();
			display_state.puzzle_text = display_state.special_cipher->encode(display_state.solution_text);
			if (display_state.special_cipher->name == "Substitution"
					|| display_state.special_cipher->name == "Shaper"
					|| display_state.special_cipher->name == "CSMajor")
			{
				cs_open = true;
				editingBox = tex_cs_ptr;
				editStr = display_state.puzzle_text;
				cursor_pos = 0;
				cursor_pos_ui = 0;
				editMode = true;
				display_state.status = INPUT;
				clear_png(tex_cs_ptr);
				render_text(tex_cs_ptr, editStr, green, 'd');
				update_texture(tex_cs_ptr);

				getTexture(textures, "textures/mini_puzzle_panel.png")->visible = true;
				getTexture(textures, "textures/submitbutton.png")->visible = true;
				
			} else {
				for (auto tex : textures)
				{
					if (tex->alignment == MiddlePane || tex->alignment == MiddlePaneBG)
					{
						tex->visible = true;
					}

					if (tex->alignment == MiddlePaneSelected)
					{
						tex->visible = false;
					}
				}

			}

			
		
			tex_minipuzzle_ptr->visible = true;
			display_state.status = WAIT_FOR_SOLVE;

		} else if (panel == "special")
		{
			display_state.special_cipher = characters[parsed[3]].species;
			display_state.special_original_text = parsed[4];
			display_state.special_request_text = display_state.special_cipher->encode(display_state.special_original_text);
			for (auto tex : textures)
			{
				if (tex->path == "textures/special_request_collapsed.png")
				{
					tex->visible = true;
				}
			}
		}
		display_state.status = CHANGING;
	} else if (keyword == "Reset_Character_Cipher") {
		if (parsed[2] == player_id) {
			// do nothing, why would the player have a cipher
		}
		else {
			if (characters.find(parsed[2]) != characters.end()) {
				characters[parsed[2]].species->reset_features();
			}
		}
		display_state.status = CHANGING;
	} else if (keyword == "Reset_Progress") {
		display_state.progress_cipher = new ToggleCipher();
		display_state.status = CHANGING;
	}

	// jump-modifying keywords
	if (keyword == "Choice_Text") {
		uint32_t count = atoi(parsed[2].c_str());
		display_state.cipher = parsed[3][0];
		display_state.jump_names.clear();
		display_state.jumps.clear();
		for (uint32_t i = 0; i < count; i++) {
			display_state.jump_names.push_back(parsed[4 + i]);
			display_state.jumps.push_back(atoi(parsed[4 + count + i].c_str()));
		}

		display_state.status = CHOICE_TEXT;
	}
	else if (keyword == "Choice_Image") {
		// TODO
		display_state.status = CHOICE_IMAGE;
	}
	else if (keyword == "Sound") {
		/*if (curr_sound != nullptr && !curr_sound->stopping)
		{
			curr_sound->stop(1.0f);
		} */

		//if (curr_sound == nullptr || curr_sound->stopped) 
		{
			std::string comp = parsed[2].c_str();
			if (comp == "door.opus"){
				//curr_sound->stopping = false;
				curr_sound.emplace_back(Sound::play(*door, 0.3f, 0.0f));
			} else if (comp == "ding.opus") {
				//curr_sound->stopping = false;
				curr_sound.emplace_back(Sound::play(*ding, 0.3f, 0.0f));
			} else if (comp == "chaos.opus") {
				//curr_sound->stopping = false;
				curr_sound.emplace_back(Sound::play(*chaos, 0.3f, 0.0f));
			} else if (comp == "docking.opus") {
				//curr_sound->stopping = false;
				curr_sound.emplace_back(Sound::play(*docking, 0.3f, 0.0f));
			} else if (comp == "chomp.opus"){
				//curr_sound->stopping = false;
				curr_sound.emplace_back(Sound::play(*chomp, 0.3f, 0.0f));
			}
			clean_curr();
		} 
		display_state.jumps = {display_state.line_number + 1};
		display_state.status = CHANGING;
	}
	else if (keyword == "Jump") {
		display_state.jumps = {(uint32_t)atoi(parsed[2].c_str())};
		display_state.status = CHANGING;
	}
	else if (keyword == "Input_Puzzle") {
		display_state.cipher = parsed[2][0];
		display_state.bottom_text = parsed[3];
		// something similar but with text input like we discussed
		correctStr = parsed[4];
		cj = (uint32_t)atoi(parsed[5].c_str());
		ij = (uint32_t)atoi(parsed[6].c_str());
		//std::cout << "past here" << std::endl;
		editMode = true;
		display_state.status = INPUT;
		editingBox = &tex_box_text;
		editStr = "";
		cursor_pos = 0;
		cursor_pos_ui = 0;

		// hide cipher panel
		getTexture(textures, "textures/cipher_panel_full.png")->visible = false;
		getTexture(textures, "textures/cipher_panel.png")->visible = true;
		cheatsheet_open = false;
		tex_rev.visible = false;
	}
	// ensure we go to the next line
	else display_state.jumps = {display_state.line_number + 1}; 

	display_state.current_choice = 0; // reset choice
}

// Backspace functionality
void PlayMode::retreat_state() {
	if (!display_state.history.size()) return; // no history
	auto &[file, lnum] = display_state.history.back();
	if (file != display_state.file) {
		apply_command("-1 Change_File " + file);
	}
	display_state.history.pop_back();
	apply_command(std::to_string(lnum - 1) + " Jump " + std::to_string(lnum));
	while (display_state.status == CHANGING) advance_one_line(0);
	draw_state_text();
}

void set_size(PlayMode::TextureItem *in){
	in->size = glm::uvec2(window_width  * (in->bounds[1] - in->bounds[0]), 
	                      window_height * (in->bounds[3] - in->bounds[2]));
}

// This is the main implementation. This should advance the game's script 
// until the player needs to advance the display again.
// In other words, things like character displays should run automatically.
void PlayMode::advance_state(uint32_t jump_choice) {
	if (display_state.line_number > 0) display_state.history.push_back(std::pair(display_state.file, display_state.line_number));
	advance_one_line(jump_choice);
	while (display_state.status == CHANGING) advance_one_line(0);

	draw_state_text();
}

void PlayMode::draw_state_text() {
	// Draw different text depending on the status.
	std::string text_to_draw = "";
	if (display_state.status == CHOICE_TEXT) {
		for (std::string s : display_state.jump_names) {
			text_to_draw += s + '\n';
		}
	}
	else 
	{
		size_t index = display_state.bottom_text.find("₿");
		if (index >= 0 && display_state.cipher == 'e')
		{
			std::string name  = display_state.bottom_text.substr(0, index);
			std::string enc_message =
				display_state.progress_cipher->encode(
				display_state.special_cipher->encode(
					display_state.bottom_text.substr(index, display_state.bottom_text.length()
					- index)));


			text_to_draw = name + enc_message;

		} else {
			text_to_draw = display_state.bottom_text;
		}
	}

	//tex_box_text.size = glm::uvec2(render_width, render_height);
	tex_box_text.bounds = {-1.0f, 1.0f, -1.0f, -0.33f, 0.0f};
	tex_box_text.margin = glm::uvec2(FONT_SIZE, FONT_SIZE);
	tex_box_text.align = MIDDLE;
	set_size(&tex_box_text);
	current_line = text_to_draw;
	render_text(&tex_box_text, text_to_draw, white, display_state.cipher, FONT_SIZE);
	update_texture(&tex_box_text);

	tex_textbg.path = textbg_path;
	tex_textbg.loadme = true;
	tex_textbg.bounds = {-1.0f, 1.0f, -1.0f, -0.33f, 0.00001f};
	set_size(&tex_textbg);
	update_texture(&tex_textbg);

	//tex_special.size = glm::uvec2(800, 400);
	tex_special.bounds = {-0.95f, -0.6f, 0.03f, 0.7f};
  	set_size(&tex_special);
	render_text(&tex_special, display_state.special_request_text, white, display_state.cipher, 48);
	update_texture(&tex_special);

	tex_minipuzzle.size = glm::uvec2(400, 100);
	tex_minipuzzle.bounds = {-0.15f, 0.15f, 0.15f, 0.3f};
	tex_minipuzzle.align = MIDDLE;
	set_size(&tex_minipuzzle);
	render_text(&tex_minipuzzle, display_state.puzzle_text, white, display_state.cipher);
	update_texture(&tex_minipuzzle);

	//tex_cs.size = glm::uvec2(render_width, render_height);
	//tex_cs.size = glm::uvec2(800, 200);
	tex_cs.bounds = {-0.15f, 0.15f, 0.08f, 0.20f, -0.00001f};
	tex_cs.align = MIDDLE;
	set_size(&tex_cs);
	update_texture(&tex_cs);

	tex_rev.bounds = {0.35f, 0.95f, 0.0f, 0.6f, -0.00001f};
	set_size(&tex_rev);
	std::string cipher_string = display_state.special_cipher->name == "Substitution"
					|| display_state.special_cipher->name == "Shaper" 
					|| display_state.special_cipher->name == "CSMajor" ?
					 "abcdefghijklmnopqrstuvwxyz₣" + std::string(substitution_display)  : "DROW₣WORD";
	if (!cheatsheet_open)
	{
		render_text(&tex_rev, cipher_string, white, display_state.cipher, 48);
	}
	update_texture(&tex_rev);
}

PlayMode::~PlayMode() {
}

void PlayMode::check_jump(std::string input, std::string correct, uint32_t correctJump, uint32_t incorrectJump){
	if (input == correct) {
		// Jump to correct line
		display_state.jumps = {correctJump};
		display_state.status = CHANGING;
		correctStr = "";
		curr_sound.emplace_back(Sound::play(*successful, 0.3f, 0.0f));

		substitution_display = substitution_display_default; // reset this in advance
	} else {
		// Jump to incorrect line
		display_state.jumps = {incorrectJump};
		display_state.status = CHANGING;
		correctStr = "";
		curr_sound.emplace_back(Sound::play(*fail, 0.3f, 0.0f));
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	// perform the initial scaling of UI textures because we only have the window_size here 
	if (!hasRescaled) {
		rescaleTextures(textures, window_size);
		hasRescaled = true;
	}

	if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_F3) {
		retreat_state();
		return true;
	} else if (evt.type == SDL_KEYDOWN && !editMode) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
			
		}
	} else if (evt.type == SDL_KEYDOWN) {
		//Edit Mode
		if (evt.key.keysym.sym == SDLK_RETURN) {
			if (editStr != "") {
				std::cout << "Sent " << editStr << " as input" << std::endl;
				
				
				if (!cs_open && !cheatsheet_open) {	
					if (correctStr != "") {
						check_jump(editStr, correctStr, cj, ij);
					}
					clear_png(editingBox);
					advance_state(display_state.current_choice);
				}

			
				
				/**/
				return true;
			}

		} else if (evt.key.keysym.sym == SDLK_LEFT) {
			if (cursor_pos > 0) {
				cursor_pos -= 1;
			} else if (cursor_pos == 0 && editingBox != &tex_box_text) {
				if (tex_minipuzzle.visible){
					cursor_pos = uint32_t(editStr.length()-1);
				} else {
					cursor_pos = uint32_t(editStr.length()-2);
				}
			}

			if (cursor_pos_ui > 0) {
				cursor_pos_ui -= 1;
			} else if (cursor_pos_ui == 0 && editingBox != &tex_box_text) {
				if (tex_minipuzzle.visible){
					cursor_pos_ui = uint32_t(editStr_ui.length()-1);
				} else {
					cursor_pos_ui = uint32_t(editStr_ui.length()-2);
				}
			}
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			if (cursor_pos < (editStr.length()-1)){
				//std::cout << std::to_string(editStr.length()) << " vs " << std::to_string(cursor_pos) << std::endl;
				cursor_pos += 1;
			} else if (cs_open) {
				cursor_pos = 0;
				cursor_pos_ui = 0;
			}

			if (cursor_pos_ui < (editStr_ui.length()-1)){
				//std::cout << std::to_string(editStr_ui.length()) << " vs (ui) " << std::to_string(cursor_pos_ui) << std::endl;
				if (cheatsheet_open && (cursor_pos_ui >= (editStr_ui.length()-2))) { // admittedly stupid but brain no work
					cursor_pos = 0;
					cursor_pos_ui = 0;
				}else{
					cursor_pos_ui += 1;
				}
			} else if (cheatsheet_open) {
				cursor_pos = 0;
				cursor_pos_ui = 0;
			}
		}
		else {
			std::string in = "";
			bool success = false;
			switch (evt.key.keysym.sym) {
				case SDLK_a: in = "A"; success = true; break;
				case SDLK_b: in = "B"; success = true; break;
				case SDLK_c: in = "C"; success = true; break;
				case SDLK_d: in = "D"; success = true; break;
				case SDLK_e: in = "E"; success = true; break;
				case SDLK_f: in = "F"; success = true; break;
				case SDLK_g: in = "G"; success = true; break;
				case SDLK_h: in = "H"; success = true; break;
				case SDLK_i: in = "I"; success = true; break;
				case SDLK_j: in = "J"; success = true; break;
				case SDLK_k: in = "K"; success = true; break;
				case SDLK_l: in = "L"; success = true; break;
				case SDLK_m: in = "M"; success = true; break;
				case SDLK_n: in = "N"; success = true; break;
				case SDLK_o: in = "O"; success = true; break;
				case SDLK_p: in = "P"; success = true; break;
				case SDLK_q: in = "Q"; success = true; break;
				case SDLK_r: in = "R"; success = true; break;
				case SDLK_s: in = "S"; success = true; break;
				case SDLK_t: in = "T"; success = true; break;
				case SDLK_u: in = "U"; success = true; break;
				case SDLK_v: in = "V"; success = true; break;
				case SDLK_w: in = "W"; success = true; break;
				case SDLK_x: in = "X"; success = true; break;
				case SDLK_y: in = "Y"; success = true; break;
				case SDLK_z: in = "Z"; success = true; break;
				case SDLK_COMMA: in = ","; success = !cs_open; break;
				case SDLK_SPACE: in = " "; success = !cs_open; 
					if (cs_open){
						editStr[cursor_pos] = in[0];
						//std::cout << editStr << std::endl;
						//std::cout << "Reached here 1 " << std::to_string(tex_minipuzzle.visible) << std::endl;
						if (cursor_pos < (editStr.length()-2)){
							cursor_pos+=1;
						} else if (cursor_pos == (editStr.length()-2)) {
							cursor_pos = 0;
						}
					}

					if (cheatsheet_open){
			
						editStr_ui[cursor_pos] = in[0];
						//std::cout << "Reached here 2 " << std::to_string(tex_minipuzzle.visible) << std::endl;
						if (cursor_pos_ui < (editStr_ui.length()-2)){
							cursor_pos_ui+=1;
						} else if (cursor_pos_ui == (editStr_ui.length()-2)) {
							cursor_pos_ui =0;
						}
					}
					break;
				case SDLK_PERIOD: in = "."; success = !cs_open; break;
				default: break;
			}
			if (success) {

				// play key click sound
				{
					/*if (curr_sound != nullptr && !curr_sound->stopping)
					{
						curr_sound->stop(1.0f);

					} 

					if (curr_sound == nullptr || curr_sound->stopped) {
						float rand_vol = 1.0f -  (rand()%30)/100.0f;
						curr_sound = Sound::play(*keyclick1, rand_vol, 0.0f); 
					}*/
					float rand_vol = 1.0f -  (rand()%30)/100.0f;
					curr_sound.emplace_back(Sound::play(*keyclick1, rand_vol, 0.0f));
					clean_curr();

					//I'm testing having infinite sounds
				}
				if (cs_open) {
					editStr[cursor_pos] = in[0];
					//std::cout << editStr << std::endl;
					if (cursor_pos < editStr.length()-1) {
						cursor_pos+=1;
					} else {
						cursor_pos = 0;
					}
				}  else if (cheatsheet_open)
				{	
					editStr_ui[cursor_pos_ui] = (char)tolower(in[0]);
					// just recalculate the entire cipher lol
					substitution_display[cursor_pos_ui] = (char)tolower(char(in[0]));
					// if ('A' <= in[0] && in[0] <= 'Z')
					display_state.progress_cipher->features["substitution"].alphabet[cursor_pos_ui] = 
						(char)tolower(in[0]);

					if (cursor_pos_ui < editStr_ui.length()-2){
						cursor_pos_ui+=1;
					} else {
						cursor_pos_ui = 0;
					}

					display_state.special_request_text = display_state.progress_cipher->encode(
						display_state.special_cipher->encode(
						display_state.special_original_text));
					draw_state_text();
					
				} else {
					editStr.insert(cursor_pos, in);
					cursor_pos += 1;

					editStr_ui.insert(cursor_pos_ui, in);
					cursor_pos_ui += 1;

				}
				
				
			}
			if (!cs_open && !cheatsheet_open && evt.key.keysym.sym == SDLK_BACKSPACE && cursor_pos > 0) {
				editStr = editStr.substr(0, cursor_pos-1) + 
				          editStr.substr(cursor_pos, editStr.length() - cursor_pos);
				float rand_vol = 1.0f -  (rand()%30)/100.0f;
				curr_sound.emplace_back(Sound::play(*keyclick1, rand_vol, 0.0f));
				clean_curr();
				cursor_pos -= 1;
			}
		} 

		if (cs_open) {

				clear_png(&tex_cs);
				render_text(&tex_cs, editStr, green, 'd');
				update_texture(&tex_cs);
				clear_png(&tex_box_text);
				render_text(&tex_box_text, current_line, white, display_state.cipher);
				update_texture(&tex_box_text);
			
		}	else if (cheatsheet_open) {
				clear_png(tex_rev_ptr);
				render_text(tex_rev_ptr, "abcdefghijklmnopqrstuvwxyz₣" + editStr_ui, green, 'd', 48);
				update_texture(tex_rev_ptr);
				clear_png(&tex_box_text);
				render_text(&tex_box_text, current_line, white, display_state.cipher);
				update_texture(&tex_box_text);

			
		}else{
			clear_png(editingBox, editingBox->size.x, editingBox->size.y);
			render_text(editingBox, editStr.substr(0, cursor_pos) + "|" +
			            editStr.substr(cursor_pos, editStr.length() - cursor_pos), white, 'd');
			update_texture(editingBox);
		}

	} else if (evt.type == SDL_KEYUP && !editMode) {
		if (evt.key.keysym.sym == SDLK_F1) {
			// toggle the left (inventory) pane
			return true;
		} else if (evt.key.keysym.sym == SDLK_F2) {
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			downArrow.pressed = false;
			size_t choices = display_state.jumps.size();
			if (choices > 0) {
				if (display_state.current_choice < choices - 1) {
					display_state.current_choice++;	
					clear_png(&tex_box_text);
					render_text(&tex_box_text, display_state.jump_names[display_state.current_choice], 
					            white, display_state.cipher);
					update_texture(&tex_box_text);
				}
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			upArrow.pressed = false;
			if (display_state.jumps.size() > 0
			 && display_state.current_choice > 0) {
				display_state.current_choice--;
			
				clear_png(&tex_box_text);
				render_text(&tex_box_text, display_state.jump_names[display_state.current_choice], 
							white, display_state.cipher);
				update_texture(&tex_box_text);
			}
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {

		// convert motion to texture coordinate system
		float tex_x = 2.0f*(((float)evt.motion.x)/window_size.x)-1.0f;
		float tex_y = -2.0f*(((float)evt.motion.y)/window_size.y)+1.0f;

		bool isLocked = checkForClick(textures, tex_x, tex_y, 
					(selected_character && selected_character->joining_line));

		// only advance if click inside of dialogue
		if ((selected_character == nullptr || !(selected_character->joining_line)) &&
			!isLocked && display_state.status != INPUT &&
			display_state.status != WAIT_FOR_SOLVE &&
			tex_x >= tex_textbg.bounds[0] &&
			tex_x < tex_textbg.bounds[1] &&
			tex_y >= tex_textbg.bounds[2] &&
			tex_y < tex_textbg.bounds[3])
		{
			clear_png(&tex_box_text);
			advance_state(0);

			// play advance sound
			/*if (curr_sound != nullptr && !curr_sound->stopping)
			{
				curr_sound->stop(1.0f);
			} 

			if (curr_sound == nullptr || curr_sound->stopped)
			{
				curr_sound = Sound::play(*keyclick2, 0.3f, 0.0f);
			}*/
			curr_sound.emplace_back(Sound::play(*keyclick2, 0.3f, 0.0f));
			clean_curr();
		}

	} else if (evt.type == SDL_MOUSEMOTION) {
		// float tex_x = 2.0f*(((float)evt.motion.x)/window_size.x)-1.0f;
		// float tex_y = -2.0f*(((float)evt.motion.y)/window_size.y)+1.0f;

		// std::cout << tex_x << ", " << tex_y << std::endl;
	}

	return false;
}

void PlayMode::update(float elapsed) {

	// move creechurs
	for (std::unordered_map<std::string, GameCharacter>::iterator c = characters.begin(); c != characters.end(); c++) {
		GameCharacter *gc = &(c->second);
		
		if (!gc->joining_line && !gc->leaving_line) {
			// printf("update: neither joining nor leaving\n");
			continue;
		}

		if (!(gc->character_completed) && 
			getTexture(textures, "textures/customer_" + gc->id + ".png")->alignment == TopMiddlePaneHidden && 
			getTexture(textures, "textures/customer_" + gc->id + + "_selected"+ ".png")->alignment == TopMiddlePaneHidden)
		{
			getTexture(textures, "textures/customer_" + gc->id + ".png")->alignment = TopMiddlePane;
			getTexture(textures, "textures/customer_" + gc->id + "_selected.png")->alignment = TopMiddlePaneSelected;
			if (!getTexture(textures, "textures/customer_" + gc->id + ".png")->visible 
			 && !getTexture(textures, "textures/customer_" + gc->id + + "_selected"+ ".png")->visible)
			{
				getTexture(textures, "textures/customer_" + gc->id + + "_selected.png")->visible = true;

				if (prev_character != "")
				{
					getTexture(textures, "textures/customer_" + prev_character + "_selected.png")->visible = false;
					getTexture(textures, "textures/customer_" + prev_character + ".png")->visible = true;
				} 
				selected_character = gc;
				prev_character = gc->id;
			}
		}

		Scene::Transform *xform = creature_xforms[gc->asset_idx];

		if (gc->joining_line == 1) {
			// printf("update: joining\n");
			if (xform->position.x < x_by_counter) {
				xform->position.x += creature_speed * elapsed;
			} else {
				c->second.joining_line = 0;
				if (c->second.leaving_line == 2) c->second.leaving_line = 1;
			}
		}
		else if (gc->leaving_line == 1) { // gc.leaving_line == true
			// printf("update: leaving. pos: (%f, %f)\n", xform->position.x, xform->position.y);
			xform->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(0., 0., 1.));
			if (xform->position.y < y_exited_store) {
				xform->position.y += creature_speed * elapsed;
			} else {
				xform->position.x = x_entering_store;
				xform->position.y = 0.;
				xform->rotation = glm::angleAxis(0.f, glm::vec3(0., 0., 1.));
				c->second.leaving_line = 0;
				if (c->second.joining_line == 2) c->second.joining_line = 1;
			}
		} else {
			printf("funny situation. hopefully shouldn't happen. joining = %d, leaving = %d\n", gc->joining_line, gc->leaving_line);
		}
	}

	updateTextures(textures);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	// //make sure framebuffers are the same size as the window:
	// framebuffers.realloc(drawable_size);

	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.f)));
	glUniform3fv(lit_color_texture_program->colorscheme_vec3_6, 18, colorscheme.data());
	glUseProgram(0);

	//---- draw scene to HDR framebuffer ----
	// glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.hdr_fb);

	glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	// glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//copy scene to main window framebuffer:
	// framebuffers.tone_map();

	glDepthFunc(GL_ALWAYS);

	drawTextures(textures, texture_program);

	// //Code taken from Jim's copied code + Sashas
	{ //texture example drawing
	
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(texture_program->program);
		glActiveTexture(GL_TEXTURE0);
		glBindVertexArray(tex_textbg.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_textbg.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_textbg.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_textbg.count);


		if (tex_special.visible)
		{
			glUseProgram(texture_program->program);
			glActiveTexture(GL_TEXTURE0);
			glBindVertexArray(tex_special.tristrip_for_texture_program);
			glBindTexture(GL_TEXTURE_2D, tex_special.tex);
			glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_special.CLIP_FROM_LOCAL) );
			glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_special.count);
		}

		if (tex_minipuzzle.visible)
		{
			glUseProgram(texture_program->program);
			glActiveTexture(GL_TEXTURE0);
			glBindVertexArray(tex_minipuzzle.tristrip_for_texture_program);
			glBindTexture(GL_TEXTURE_2D, tex_minipuzzle.tex);
			glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_minipuzzle.CLIP_FROM_LOCAL) );
			glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_minipuzzle.count);
		}

		if (tex_rev.visible)
		{
			glUseProgram(texture_program->program);
			glActiveTexture(GL_TEXTURE0);
			glBindVertexArray(tex_rev.tristrip_for_texture_program);
			glBindTexture(GL_TEXTURE_2D, tex_rev.tex);
			glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_rev.CLIP_FROM_LOCAL) );
			glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_rev.count);
		}

		glBindVertexArray(tex_box_text.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_box_text.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_box_text.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_box_text.count);
		GL_ERRORS();

		if (cs_open)
		{
			glBindVertexArray(tex_cs.tristrip_for_texture_program);
			glBindTexture(GL_TEXTURE_2D, tex_cs.tex);
			glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_cs.CLIP_FROM_LOCAL) );
			glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_cs.count);
			GL_ERRORS();
		}
	
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_BLEND);
	}

	GL_ERRORS();
}