#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "TextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <iostream>
#include <fstream>
#include <random>

#define FONT_SIZE 36
#define TOPMARGIN (FONT_SIZE * .5)
#define LEFTMARGIN (FONT_SIZE * 2)
#define LINE_SPACING (FONT_SIZE * 0.25)

// Storing font here, we can also refactor the text storage to load a font path as the first line per script or have a font buffer.

static constexpr std::string tex_path = "out.png";
static constexpr std::string textbg_path = "textbg.png";
//static constexpr std::string bg_path = "black.png";
static std::string font_path = "SpaceGrotesk-Regular.ttf";
static constexpr uint32_t window_height = 720;
static constexpr uint32_t window_width = 1280;
static constexpr uint32_t render_height = window_height/3;
static constexpr uint32_t render_width = window_width;
static glm::u8vec4 text_render[render_height][render_width];
static std::vector<std::string> activeScript;
static uint32_t activeIndex = 0;
static uint32_t lastIndex = 0;
static int choices = 0;
static std::vector<std::string> links;
static bool editMode = false;
static std::string editStr = "";
static uint32_t cursor_pos = 0;
static char substitution[26] = {'b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','a'};

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

//Reset the current text (or other) png
void clear_png(glm::u8vec4 *png_in, uint32_t height, uint32_t width) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            png_in[y*width + x] = glm::u8vec4(0, 0, 0, 0);  // RGBA = (0, 0, 0, 0) = transparent black
        }
    }
}

//Draw text in png format from bitmap into out. Takes a color to draw into. 
void draw_png(FT_Bitmap *bitmap, glm::u8vec4 *out, uint32_t x, uint32_t y, uint32_t height, uint32_t width, glm::u8vec4 color) {
	for (uint32_t i = 0; i < bitmap->rows; i++) {
        for (uint32_t j = 0; j < bitmap->width; j++) {
            uint32_t out_x = x + j;
        	uint32_t out_y = y + i;//bitmap->rows - i - 1;
            if (out_x < width && out_y < height) {
				// According to freetype.org,  buffer bytes per row is saved as bitmap->pitch
                uint8_t alpha = bitmap->buffer[i * bitmap->pitch + j];
                if (alpha > 0) {
                    // Calculate the index in the RGBA buffer
                    int index = out_y * width + out_x;
					
					out[index] = glm::u8vec4(color.r, color.g, color.b, alpha);
                }
            }
        }
    }
}

std::string decode(std::string str_in, char key){
	std::string out = "";
	switch (key){
		case 'e':
			for (int i = 0; i < str_in.length(); i++){
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
			break;
	}
	//std::cout << "from " << str_in << " to " << out << std::endl;
	return out;
}

//Nightmare loop, takes text and a color and turns it into a png of text in that color.
void render_text(PlayMode::TextureItem *tex_in, std::string line_in, glm::u8vec4 color) {
	choices = 0;
	links; //idk why I did this

	glm::u8vec4 colorOut = color; //Overriding it if it's a choice because I want to :D
	std::string line = "";

	//I wanted to get special characters done before glyphs were in so this was the easiest solution I could find
	//Numbers are unicode encodings for my keywords in the script. Change these later if we're using a different method.
	//TODO (Optional): Move newlines here too. Matias/Jim mentioned I should probably handle them before I generate glyphs
	//but not 100% on how
	
	//if line_in's first character is ₽, it's a choice.
	if (line_in.length() >= 3 && line_in[0] == -30 && line_in[1] == -126 && line_in[2] == -67 ) {
		choices += 1;
		bool display_mode = true; //This determines if current block should be rendered
		std::string link_now = "";
		int i = 3;
		while(i < line_in.length()-2){
			//new link, checking for ₼
			if (line_in[i] == -30 && line_in[i+1] == -126 && line_in[i+2] == -68){
				display_mode = false;
				line = line + "₿";
				i+=3;
				continue;
			}
			//new choice (text), checking for ₽ again (same as initial if)
			if (line_in[i] == -30 && line_in[i+1] == -126 && line_in[i+2] == -67){
				display_mode = true;
				links.emplace_back(link_now);
				choices += 1;
				link_now = "";
				line = "Up: " + line + "Down: ";
				i+=3;
				continue;
			}
			// If text, display it, if not, it's a link, so add it to the list of links.
			if (display_mode){
				line = line + line_in[i];
			} else {
				link_now = link_now + line_in[i];
			}
			i++;
		}
		if (!display_mode){
			links.emplace_back(link_now);
		}
	}
	if (choices == 0) {
		line = line_in;
	} else {
		colorOut = glm::u8vec4(0,0,255,1);
	}

	line = decode(line, 'e');
	
	// Based on Harfbuzz example at: https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	// since the below code follows the code from the example basically exactly, I'm also including some annotations
	// of my understanding of what's going on
	FT_Library ft_library;
	FT_Face ft_face;
	FT_Error ft_error;

	// Initialize Freetype basics and check for failure
	// Load freetype library into ft_library
	if ((ft_error = FT_Init_FreeType (&ft_library))){ 
		std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
		std::cout << "Init Freetype Library failed, aborting..." << std::endl;
		abort();
	}
	// Load font face through path (font_path)
	if ((ft_error = FT_New_Face (ft_library, data_path(font_path).c_str(), 0, &ft_face))){ // .c_str() converts to char *
		std::cout << "Failed while loading " << data_path(font_path).c_str() << std::endl;
		std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
		std::cout << "Init Freetype Face failed, aborting..." << std::endl;
		abort();
	}
	// Define a character size based on constant literals
	if ((ft_error = FT_Set_Char_Size (ft_face, FONT_SIZE*64, FONT_SIZE*64, 0, 0))){
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
	
	//Commented code is for debugging, should that be necessary.
	/*printf ("Raw buffer contents:\n");
	for (unsigned int i = 0; i < len; i++)
	{
		hb_codepoint_t gid   = info[i].codepoint;
		unsigned int cluster = info[i].cluster;
		double x_advance = pos[i].x_advance / 64.;
		double y_advance = pos[i].y_advance / 64.;
		double x_offset  = pos[i].x_offset / 64.;
		double y_offset  = pos[i].y_offset / 64.;

		char glyphname[32];
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

		printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g)\n",
				glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
	}*/

	// Following is derived from the Freetype example at https://freetype.org/freetype2/docs/tutorial/step1.html
	FT_GlyphSlot  slot = ft_face->glyph; // 
	int           pen_x, pen_y;
	static uint32_t h = window_height/3;

	pen_x = static_cast<int>(LEFTMARGIN);
	pen_y = static_cast<int>(TOPMARGIN) + FONT_SIZE;

	double line_height = FONT_SIZE;
	bool lastWasSpace = true;
	char glyphname[32];
	for (uint32_t n = 0; n < len; n++ ) {
		hb_codepoint_t gid   = info[n].codepoint;
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));
		// unsigned int cluster = info[n].cluster;
		double x_position = pen_x + pos[n].x_offset / 64.;
		double y_position = pen_y + pos[n].y_offset / 64.;

		// Cumbersome but this lets us automatically handle new lines.
		float wordWidth = 0.0f;
		std::string word = "";
		char loop_glyphname[32] = "";
		if (lastWasSpace) {
			uint32_t m = n;
			while (strcmp(loop_glyphname,"space") != 0 && strcmp(loop_glyphname,"uni20BF") != 0 && m < len){
				hb_codepoint_t loop_gid  = info[m].codepoint;
				hb_font_get_glyph_name (hb_font, loop_gid, loop_glyphname, sizeof (loop_glyphname));
				word = word + loop_glyphname;
				wordWidth += pos[m].x_advance / 64;
				m+=1;
			}

			if (x_position + wordWidth >= window_width - 2*LEFTMARGIN) {
				pen_x = static_cast<int>(LEFTMARGIN); 
				pen_y += static_cast<int>(line_height + LINE_SPACING); 
				x_position = pen_x + pos[n].x_offset / 64.;
				y_position = pen_y + pos[n].y_offset / 64.;

				line_height = FONT_SIZE;
			}
		}
		
		// The above solution to lines does not allow for \n since it's based in glyphs.
		// Instead of \n,  I will be using ₿ to indicate line ends.
		// I've adjusted my text asset pipeline to account for this.
		// NOTE: This is probably why Matias/Jim told me to process it before glyphifying, lol
		if (strcmp(glyphname, "uni20BF") == 0) {
			pen_x = static_cast<int>(LEFTMARGIN); 
			pen_y += static_cast<int>(line_height + LINE_SPACING); 
			x_position = pen_x + pos[n].x_offset / 64.;
			y_position = pen_y + pos[n].y_offset / 64.;

			line_height = FONT_SIZE;
			continue;
		}

		
		// load glyph image into the slot (erase previous one) 
		FT_Error error = FT_Load_Glyph(ft_face, gid, FT_LOAD_RENDER); // Glyphs instead of Chars
		if (error) continue; 

		// track max line_height for unification
		if (slot->bitmap.rows > line_height) {
			line_height = slot->bitmap.rows;
		}
		
		// bitmap drawing function
		draw_png(&slot->bitmap, &text_render[0][0], static_cast<int>(x_position + slot->bitmap_left), static_cast<int>(y_position - slot->bitmap_top), h, window_width, colorOut);

		// Move the "pen" based on x and y advance given by glyphs
		pen_x += pos[n].x_advance / 64; 
		pen_y += pos[n].y_advance / 64; 

		if (strcmp(glyphname,"space")==0){
			lastWasSpace = true;
		} else {
			lastWasSpace = false;
		}
	}
	//This is a disk-heavy line and not quite necessary.
	//Originally did this in part for debugging purposes.
	//TODO: Figure out how to store this in a variable and not on disk, lol.
	//save_png(data_path("out.png"), glm::uvec2(window_width, h), &text_render[0][0], UpperLeftOrigin); //Upper left worked.
	
	std::vector<glm::u8vec4> data_out;

	//Based on https://cplusplus.com/forum/beginner/190966/
	for (int row = render_height-1; row > -1; row--)
	{
		for (int col = 0; col < render_width; col++)
		{
			data_out.push_back(text_render[row][col]);
		}
	}

	tex_in->data = data_out;
	tex_in->size = glm::uvec2(window_width, h);

	hb_buffer_destroy (hb_buffer);
	hb_font_destroy (hb_font);

	FT_Done_Face (ft_face);
	FT_Done_FreeType (ft_library);
}

//x0,x1,y0,y1,z encode the coords for where on the screen the texture is.
int update_texture(PlayMode::TextureItem *tex_in, float x0, float x1, float y0, float y1, float z){
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

		glVertexAttribPointer( texture_program->Position_vec4, 3, GL_FLOAT, GL_FALSE, sizeof(PlayMode::PosTexVertex), (GLbyte *)0 + offsetof(PlayMode::PosTexVertex, Position) );
		glEnableVertexAttribArray( texture_program->Position_vec4 );

		glVertexAttribPointer( texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(PlayMode::PosTexVertex), (GLbyte *)0 + offsetof(PlayMode::PosTexVertex, TexCoord) );
		glEnableVertexAttribArray( texture_program->TexCoord_vec2 );

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	GL_ERRORS();

	std::vector< PlayMode::PosTexVertex > verts;
	
	// Define top right quadrant to full texture
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x0, y0, z),
		.TexCoord = glm::vec2(0.0f, 0.0f), // extra comma lets you make a 1 line change later
	});
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x0, y1, z),
		.TexCoord = glm::vec2(0.0f, 1.0f),
	});
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x1, y0, z),
		.TexCoord = glm::vec2(1.0f, 0.0f),
	});
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x1, y1, z),
		.TexCoord = glm::vec2(1.0f, 1.0f),
	});
	/*for (int i = 0; i < verts.size(); i++) {
		std::cout << glm::to_string(verts[i].Position) << std::endl;
		std::cout << glm::to_string(verts[i].TexCoord) << std::endl;
	}*/
	glBindBuffer(GL_ARRAY_BUFFER, tex_in->tristrip);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(verts[0]), verts.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	tex_in->count = GLsizei(verts.size());
	GL_ERRORS();

	
	//identity transform (just drawing "on the screen"):
	tex_in->CLIP_FROM_LOCAL = glm::mat4(1.0f);

	
	//camera transform (drawing "in the world"):
	/*tex_in.CLIP_FROM_LOCAL = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local()) * glm::mat4(
		5.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 5.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 5.0f, 0.0f,
		0.0f, 0.0f, 20.0f, 1.0f
	);*/
	
	GL_ERRORS();
	return 0;
}

//Using filestreams - maybe don't use this in final
void loadScript (std::string path_in){
	// following partially adopted from example in https://cplusplus.com/doc/tutorial/files/
	std::string line;
	std::ifstream thisScript (data_path("script/" + path_in));
	activeScript.clear();
	activeIndex = 0;
	if (thisScript.is_open()) {
		while ( getline (thisScript,line, '\r') ){
			activeScript.emplace_back(line);
		}
		thisScript.close();
	}
	else { //script failed to load
		std::cout << "not sure how to handle this lol" << std::endl; 
	}
}

void advance_step (uint32_t x, uint32_t y){
	if (activeScript[activeIndex] != activeScript.back()) activeIndex += 1;
}
void reverse_step (uint32_t x, uint32_t y){
	if (activeScript[activeIndex] != activeScript.front()) activeIndex -= 1;
}

PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);

	loadScript("start.txt");

	render_text(&tex_example, activeScript[activeIndex], white);
	
	update_texture(&tex_example, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
	tex_textbg.path = textbg_path;
	tex_textbg.loadme = true;
	update_texture(&tex_textbg, -1.0f, 1.0f, -1.0f, -0.33f, 0.0001f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN && !editMode) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
				return true;
			} else {
				SDL_SetRelativeMouseMode(SDL_FALSE);
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}else if (evt.key.keysym.sym == SDLK_RETURN) {
			//enter.pressed = false;
			editMode = !editMode;
		}
	} else if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_RETURN) {
			//enter.pressed = false;
			editMode = !editMode;
		} else if (evt.key.keysym.sym == SDLK_LEFT) {
			if(cursor_pos != 0){
				cursor_pos -= 1;
			}
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			if(cursor_pos != editStr.length()){
				cursor_pos += 1;
			}
		}
		//else if (keysym.unicode ) { // Maybe maintain and check a list of acceptable keys
		else {
			std::string in = "";
			bool success = false;
			switch (evt.key.keysym.sym) {
				case SDLK_a:
					in = "A";
					success = true;
					break;
				case SDLK_b:
					in = "B";
					success = true;
					break;
				case SDLK_c:
					in = "C";
					success = true;
					break;
				case SDLK_d:
					in = "D";
					success = true;
					break;
				case SDLK_e:
					in = "E";
					success = true;
					break;
				case SDLK_f:
					in = "F";
					success = true;
					break;
				case SDLK_g:
					in = "G";
					success = true;
					break;
				case SDLK_h:
					in = "H";
					success = true;
					break;
				case SDLK_i:
					in = "I";
					success = true;
					break;
				case SDLK_j:
					in = "J";
					success = true;
					break;
				case SDLK_k:
					in = "K";
					success = true;
					break;
				case SDLK_l:
					in = "L";
					success = true;
					break;
				case SDLK_m:
					in = "M";
					success = true;
					break;
				case SDLK_n:
					in = "N";
					success = true;
					break;
				case SDLK_o:
					in = "O";
					success = true;
					break;
				case SDLK_p:
					in = "P";
					success = true;
					break;
				case SDLK_q:
					in = "Q";
					success = true;
					break;
				case SDLK_r:
					in = "R";
					success = true;
					break;
				case SDLK_s:
					in = "S";
					success = true;
					break;
				case SDLK_t:
					in = "T";
					success = true;
					break;
				case SDLK_u:
					in = "U";
					success = true;
					break;
				case SDLK_v:
					in = "V";
					success = true;
					break;
				case SDLK_w:
					in = "W";
					success = true;
					break;
				case SDLK_x:
					in = "X";
					success = true;
					break;
				case SDLK_y:
					in = "Y";
					success = true;
					break;
				case SDLK_z:
					in = "Z";
					success = true;
					break;
				case SDLK_COMMA:
					in = ",";
					success = true;
					break;
				case SDLK_SPACE:
					in = " ";
					success = true;
					break;
				case SDLK_PERIOD:
					in = ".";
					success = true;
					break;
				default:
					break;
			}
			if (success) {
				editStr.insert(cursor_pos, in);
				cursor_pos += 1;
			}
			if (evt.key.keysym.sym == SDLK_BACKSPACE && cursor_pos > 0) {
				editStr = editStr.substr(0, cursor_pos-1) + editStr.substr(cursor_pos, editStr.length() - cursor_pos);
				cursor_pos -= 1;
			}
		} 
		clear_png(&text_render[0][0], window_height/3, window_width);
		render_text(&tex_example, editStr.substr(0, cursor_pos) + "l " + editStr.substr(cursor_pos, editStr.length() - cursor_pos), white);
		update_texture(&tex_example, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
		std::cout << editStr.substr(0, cursor_pos) << "(CURSOR)" << editStr.substr(cursor_pos, editStr.length() - cursor_pos) << std::endl;
	} else if (evt.type == SDL_KEYUP && !editMode) {
		
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			downArrow.pressed = false;
			// std::cout << std::to_string(choices) << std::endl;
			if (choices > 0){
				//std::cout << links[0] << std::endl;
				//std::cout << links[1] << std::endl;
				if (choices > 1){
					loadScript(links[1]);
					links.clear();
				}else{
					loadScript(links[0]);
					links.clear();
				}
				clear_png(&text_render[0][0], window_height/3, window_width);
				render_text(&tex_example, activeScript[activeIndex], white);
				update_texture(&tex_example, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			upArrow.pressed = false;
			if (choices > 0){
				loadScript(links[0]);
				links.clear();
				clear_png(&text_render[0][0], window_height/3, window_width);
				render_text(&tex_example, activeScript[activeIndex], white);
				update_texture(&tex_example, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
			}
			return true;
		} 
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		
		clear_png(&text_render[0][0], window_height/3, window_width);
		if (evt.button.button == SDL_BUTTON_RIGHT){
			reverse_step(evt.button.x , evt.button.y);
		} else {
			advance_step(evt.button.x , evt.button.y);
		}
		render_text(&tex_example, activeScript[activeIndex], white);
		update_texture(&tex_example, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	//Code taken from Jim's copied code + Sashas
	{ //texture example drawing
	
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(texture_program->program);
		glActiveTexture(GL_TEXTURE0);
		//std::cout << "Texture ID: " << tex_example.tex << std::endl;
		glBindVertexArray(tex_textbg.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_textbg.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_textbg.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_textbg.count);

		glBindVertexArray(tex_example.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_example.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_example.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_example.count);
		
		/*glBindVertexArray(tex_bg.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_bg.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_bg.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_bg.count);*/

		// std::cout << std::to_string(size_t(tex_example.count)) << std::endl;

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_BLEND);
	}
	//Default Line-y text code, if necessary
	/*
	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();*/
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
