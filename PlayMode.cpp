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
#include <fstream>
#include <sstream>

#define FONT_SIZE 36
#define TOPMARGIN (FONT_SIZE * .5)
#define LEFTMARGIN (FONT_SIZE * 2)
#define LINE_SPACING (FONT_SIZE * 0.25)

// Storing font here, we can also refactor the text storage to load a font path as the first line per script or have a font buffer.

static std::string tex_path = "out.png";
static std::string textbg_path = "textbg.png";
//static constexpr std::string bg_path = "black.png";
static std::string font_path = "RobotoMono-Regular.ttf";
static constexpr uint32_t window_height = 720;
static constexpr uint32_t window_width = 1280;
static constexpr uint32_t render_height = window_height/3;
static constexpr uint32_t render_width = window_width;
// static glm::u8vec4 text_render[render_height][render_width];
static std::vector<std::string> activeScript;
// static uint32_t activeIndex = 0;
// static uint32_t lastIndex = 0;
static std::vector<std::string> links;
static bool editMode = false;
static std::string editStr = "";
static uint32_t cursor_pos = 0;
static PlayMode::TextureItem* editingBox;
static bool cs_open = false;
static std::string current_line = "";
static std::string correctStr = "";
static uint32_t cj = 0;
static uint32_t ij = 0;

// Leaving the cipher up here for now because the substitution is here
ReverseCipher reverse_cipher;
static char substitution[26] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};

GLuint codename_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > codename_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("codename.pnct"));
	codename_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > codename_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("codename.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = codename_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = codename_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

//Reset the current text (or other) png
void clear_png(PlayMode::TextureItem *png_in, uint32_t height = 0, uint32_t width = 0) {
    /*for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            png_in[y*width + x] = glm::u8vec4(0, 0, 0, 0);  // RGBA = (0, 0, 0, 0) = transparent black
        }
    }*/
	for (auto i = png_in->data.begin(); i != png_in->data.end(); ++i){
		*i = glm::u8vec4(0,0,0,0);
	}
}

//Draw text in png format from bitmap into out. Takes a color to draw into. 
void draw_glyph_png(FT_Bitmap *bitmap, PlayMode::TextureItem *png_out, uint32_t x, uint32_t y, glm::u8vec4 color) {
	//auto it = png_out->data.begin();
	for (uint32_t i = 0; i < bitmap->rows; i++) {
        for (uint32_t j = 0; j < bitmap->width; j++) {
            uint32_t out_x = x + j;
        	uint32_t out_y = y + i;//bitmap->rows - i - 1;
            if (out_x < png_out->size.x && out_y < png_out->size.y) {
				// According to freetype.org,  buffer bytes per row is saved as bitmap->pitch
                uint8_t alpha = bitmap->buffer[i * bitmap->pitch + j];
				// Calculate the index in the RGBA buffer
                int index = (png_out->size.y-out_y-1) * png_out->size.x + out_x;
				//std::cout << std::to_string(index) << " from size " << std::to_string(png_out->data.size());
				png_out->data[index] = glm::u8vec4(color.r, color.g, color.b, alpha);
            }
        }
    }
	//std::cout << std::to_string(png_out->data.size());
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
			out = str_in;
			break;
	}
	//std::cout << "from " << str_in << " to " << out << std::endl;
	return out;
}

//Nightmare loop, takes text and a color and turns it into a png of text in that color.
void PlayMode::render_text(PlayMode::TextureItem *tex_in, std::string line_in, glm::u8vec4 color, char cipher = 'e', int font_size = FONT_SIZE) {
	size_t choices = display_state.jumps.size();
	// links; //idk why I did this

	glm::u8vec4 colorOut = color; //Overriding it if it's a choice because I want to :D
	std::string line = "";

	//I wanted to get special characters done before glyphs were in so this was the easiest solution I could find
	//Numbers are unicode encodings for my keywords in the script. Change these later if we're using a different method.
	//TODO (Optional): Move newlines here too. Matias/Jim mentioned I should probably handle them before I generate glyphs
	//but not 100% on how
	
	// I removed the choice checking for now. ~Yoseph
	// I'll change it in a bit, but it doesn't work for the current script purposes right now.
	
	line = line_in;
	//turn options blue
	if (choices != 1) {
		colorOut = glm::u8vec4(0,0,255,1);
	}

	line = decode(line, cipher);
	// Based on Harfbuzz example at: https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	// since the below code follows the code from the example basically exactly, I'm also including some annotations
	// of my understanding of what's going on
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
	

	// Following is derived from the Freetype example at https://freetype.org/freetype2/docs/tutorial/step1.html
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
	

	pen_x = static_cast<int>(LEFTMARGIN);
	pen_y = static_cast<int>(TOPMARGIN) + font_size;

	double line_height = font_size;
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
		//std::cout << glyphname;
		if (lastWasSpace) {
			uint32_t m = n;
			while (strcmp(loop_glyphname,"space") != 0 && strcmp(loop_glyphname,"uni20BF") != 0 && m < len){
				hb_codepoint_t loop_gid  = info[m].codepoint;
				hb_font_get_glyph_name (hb_font, loop_gid, loop_glyphname, sizeof (loop_glyphname));
				word = word + loop_glyphname;
				wordWidth += pos[m].x_advance / 64;
				m+=1;
			}

			if (x_position + wordWidth >= tex_in->size.x - 2*LEFTMARGIN) {
				pen_x = static_cast<int>(LEFTMARGIN); 
				pen_y += static_cast<int>(line_height + LINE_SPACING); 
				x_position = pen_x + pos[n].x_offset / 64.;
				y_position = pen_y + pos[n].y_offset / 64.;

				line_height = font_size;
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

			line_height = font_size;
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
		if (colorOut == green && n == cursor_pos){
			draw_glyph_png(&slot->bitmap, tex_in, static_cast<int>(x_position + slot->bitmap_left), static_cast<int>(y_position - slot->bitmap_top), white);
		} else {
			draw_glyph_png(&slot->bitmap, tex_in, static_cast<int>(x_position + slot->bitmap_left), static_cast<int>(y_position - slot->bitmap_top), colorOut);
		}

		// Move the "pen" based on x and y advance given by glyphs
		pen_x += pos[n].x_advance / 64; 
		pen_y += pos[n].y_advance / 64; 

		if (strcmp(glyphname,"space")==0){
			lastWasSpace = true;
		} else {
			lastWasSpace = false;
		}
	}
	

	hb_buffer_destroy (hb_buffer);
	hb_font_destroy (hb_font);

	FT_Done_Face (ft_face);
	FT_Done_FreeType (ft_library);
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

void PlayMode::initializeCallbacks()
{

	for (std::string path : paths)
	{
		if (path == "special_request_collapsed.png")
		{
			// icon that opens special request menu
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, LeftPane);
			};

			callbacks.emplace_back(callback);
		}
		else if (path == "special_request.png")
		{
			// special request menu
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, LeftPane);
			};

			callbacks.emplace_back(callback);
		} 
		else if (path == "cipher_panel.png")
		{
			// cipher panel button, on click expands the cipher panel
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, RightPane);
			};

			callbacks.emplace_back(callback);
		}
		else if (path == "cipher_panel_full.png")
		{
			// full cipher panel, on click collapses the cipher panel
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
				togglePanel(textures, RightPane);
			};

			callbacks.emplace_back(callback);
		} else if (path.substr(0,8) == "customer")
		{

			// special customer selector, either select or deselect customer
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				// if the name is longer than customerN.png, we know the 
				// selected version image was clicked (so we're deselecting)
				// otherwise, the deselected image was clicked so we're
				// selecting

				if (path.length() > 13)
				{
					std::cout << "deselecting customer: " << path << std::endl;
					
					
				} else {
					std::cout << "selecting customer: " << path << std::endl;

				}

				// toggle selected/deselected button look
				for (auto tex : textures)
				{

					if (tex->path != path && tex->path.substr(0,9) == path.substr(0,9))
					{
						tex->visible = true;
					} else if (tex->path == path)
					{
						tex->visible = false;
					}


				}


			};

			callbacks.emplace_back(callback);
		} else if (path == "reverse_button.png")
		{
			// button that reverses the text
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				// toggle selected version on
				for (auto tex : textures)
				{
					if (tex->path == "reverse_button.png")
					{
						tex->visible = false;

					}

					if (tex->path == "reverse_button_selected.png")
					{
						tex->visible = true;
					}
				}
				CipherFeature cf;
				cf.b = false;
				reverse_cipher.set_feature("flip", cf);
				std::string res = reverse_cipher.encode(display_state.puzzle_text);
				std::cout << res << std::endl;
				display_state.bottom_text = res;
				draw_state_text();
			};
			callbacks.emplace_back(callback);


		} else if (path == "reverse_button_selected.png")
		{
			// button that unreverses the text
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){

				// toggle unselected version on
				for (auto tex : textures)
				{
					if (tex->path == "reverse_button_selected.png")
					{
						tex->visible = false;

					}

					if (tex->path == "reverse_button.png")
					{
						tex->visible = true;
					}
				}
				CipherFeature cf;
				cf.b = true;
				reverse_cipher.set_feature("flip", cf);
				std::string res = reverse_cipher.encode(display_state.puzzle_text);
				std::cout << res << std::endl;
				display_state.bottom_text = res;
				draw_state_text();
			};

			callbacks.emplace_back(callback);

		}
		else if (path == "submitbutton.png")
		{
			// submit button for mini puzzle window
			auto callback = [&](std::vector<TexStruct *> textures, std::string path){
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
					display_state.solved_puzzle = true;
					advance_state(display_state.current_choice);
					std::cout << "Submitted" << std::endl;
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

// //Using filestreams - maybe don't use this in final
// void loadScript (std::string path_in){
// 	// following partially adopted from example in https://cplusplus.com/doc/tutorial/files/
// 	std::string line;
// 	std::ifstream thisScript (data_path("script/" + path_in));
// 	activeScript.clear();
// 	activeIndex = 0;
// 	if (thisScript.is_open()) {
// 		while ( getline (thisScript,line, '\r') ){
// 			activeScript.emplace_back(line);
// 		}
// 		thisScript.close();
// 	}
// 	else { //script failed to load
// 		std::cout << "not sure how to handle this lol" << std::endl; 
// 	}
// }

// void advance_step (uint32_t x, uint32_t y){
// 	if (activeScript[activeIndex] != activeScript.back()) activeIndex += 1;
// }
// void reverse_step (uint32_t x, uint32_t y){
// 	if (activeScript[activeIndex] != activeScript.front()) activeIndex -= 1;
// }

PlayMode::PlayMode() : scene(*codename_scene) {
	//get pointers to stuff
	for (auto &transform : scene.transforms) {
		if (transform.name == "swap_creature") swap_creature = &transform;
	}
	if (swap_creature == nullptr) throw std::runtime_error("Creature not found.");

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

	advance_state(0);
}

/* This function should refresh the display according to whatever is in the state.
 * Might be necessary at the very start.
 * Could also be useful if something goes wrong.
 */
void PlayMode::refresh_display() {
	// draw bottom_text
	// draw the characters and images displayed
	// these are all comments because I'm not sure what exactly this should look like yet
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
			if (parsed[4] == "Bleebus") {
				g.species = ReverseCipher(parsed[4]);
			}
			else {

			}
			g.asset_idx = 0; // this is the "swap creature".  @todo Change this line when we have more characters
			characters[parsed[2]] = g;
		}
		else {
			std::cerr << display_state.file + " " + parsed[0] + ": Found a character with this ID already. No action taken" << std::endl;
		}
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
		if (parsed[2] == player_id) display_state.bottom_text = parsed[3];
		else {
			if (characters.find(parsed[2]) != characters.end()) {
				if (characters[parsed[2]].species.name == "Bleebus") {
					std::string res = reverse_cipher.encode(parsed[3]);
					std::cout << res << std::endl;
					display_state.bottom_text = res;
				}
			}
			else {
				display_state.bottom_text = parsed[3];
			}
		}
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
	else if (keyword == "Solve_Puzzle") {
		if (parsed[2] == player_id) display_state.bottom_text = parsed[3];
		else {
			if (characters.find(parsed[2]) != characters.end()) {
				if (characters[parsed[2]].species.name == "Bleebus") {
					std::string res = reverse_cipher.encode(parsed[3]);
					std::cout << res << std::endl;
					display_state.bottom_text = res;
					display_state.puzzle_text = parsed[3];
				}
			}
			else {
				display_state.bottom_text = parsed[3];
			}
		}
		display_state.status = WAIT_FOR_SOLVE;
		display_state.solved_puzzle = false;
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
		display_state.jumps.push_back(0);
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
	}
	else display_state.jumps = {display_state.line_number + 1}; // ensure we go to the next line

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

// This is the main implementation. This should advance the game's script until the player needs to advance the display again.
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
	else text_to_draw = display_state.bottom_text;

	tex_box_text.size = glm::uvec2(render_width, render_height);
	tex_box_text.bounds = {-1.0f, 1.0f, -1.0f, -0.33f, 0.0f};
	current_line = text_to_draw;
	render_text(&tex_box_text, text_to_draw, white, display_state.cipher);
	update_texture(&tex_box_text);


	tex_textbg.path = textbg_path;
	tex_textbg.loadme = true;
	tex_textbg.bounds = {-1.0f, 1.0f, -1.0f, -0.33f, 0.00001f};
	update_texture(&tex_textbg);

	tex_cs.size = glm::uvec2(render_width, render_height);
	//tex_cs.size = glm::uvec2(800, 200);
	tex_cs.bounds = {-0.17f, 1.0f, 0.18f, 0.48f, -0.00001f};
	update_texture(&tex_cs);

}

PlayMode::~PlayMode() {
}

void PlayMode::check_jump(std::string input, std::string correct, uint32_t correctJump, uint32_t incorrectJump){
	if (input == correct) {
		// Jump to correct line
		display_state.jumps = {correctJump};
		display_state.status = CHANGING;
		correctStr = "";
	} else {
		// Jump to incorrect line
		display_state.jumps = {incorrectJump};
		display_state.status = CHANGING;
		correctStr = "";
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	// perform the initial scaling of UI textures because we only have the window_size here 
	if (!hasRescaled)
	{
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
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
			
		}
	} else if (evt.type == SDL_KEYDOWN) {
		//Edit Mode
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			if (cs_open) {
				togglePanel(textures, RightPane);
				clear_png(&tex_box_text);
				render_text(&tex_box_text, current_line, white, display_state.cipher);
				update_texture(&tex_box_text);
				cs_open = false;
			}
			editMode = false;
			editStr = "";
			cursor_pos = 0;
			display_state.status = CHANGING;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_RETURN) {
			//enter.pressed = false;
			//std::cout << "editmode" << std::endl;
			if (editStr != "") {
				std::cout << "Sent " << editStr << " as input" << std::endl;
				
				// checking function

				if (!cs_open) {	
					if (correctStr != ""){
						check_jump(editStr, correctStr, cj, ij);
					}
					clear_png(editingBox);
					advance_state(display_state.current_choice);
				}
				editMode = false;
				editStr = "";
				cursor_pos = 0;
				display_state.status = CHANGING;
				/**/
				return true;
			}
		} else if (evt.key.keysym.sym == SDLK_LEFT) {
			if (cursor_pos != 0) {
				cursor_pos -= 1;
			}
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			if(cursor_pos != editStr.length()){
				cursor_pos += 1;
			} else {
				if (cs_open){
					cursor_pos = 0;
				}
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
				case SDLK_SPACE: in = " "; success = !cs_open; break;
				case SDLK_PERIOD: in = "."; success = !cs_open; break;
				default: break;
			}
			if (success) {
				if (cs_open){
					editStr[cursor_pos] = in[0] - 'A' + 'a';
					substitution[cursor_pos] = in[0] - 'A' + 'a';
					//std::cout << editStr << std::endl;
					if (cursor_pos < 25){
						cursor_pos+=1;
					} else {
						cursor_pos = 0;
					}
				}else{
					editStr.insert(cursor_pos, in);
					cursor_pos += 1;
				}
			}
			if (!cs_open && evt.key.keysym.sym == SDLK_BACKSPACE && cursor_pos > 0) {
				editStr = editStr.substr(0, cursor_pos-1) + editStr.substr(cursor_pos, editStr.length() - cursor_pos);
				cursor_pos -= 1;
			}
		} 

		if (cs_open){
			clear_png(&tex_cs);
			render_text(&tex_cs, editStr, green, 'd', 75);
			update_texture(&tex_cs);
			clear_png(&tex_box_text);
			render_text(&tex_box_text, current_line, white, display_state.cipher);
			update_texture(&tex_box_text);
		}else{
			clear_png(editingBox, editingBox->size.x, editingBox->size.y);
			render_text(editingBox, editStr.substr(0, cursor_pos) + "|" + editStr.substr(cursor_pos, editStr.length() - cursor_pos), white, 'd');
			update_texture(editingBox);
		}

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
		} else if (evt.key.keysym.sym == SDLK_F1) {

			// toggle the left (inventory) pane
			
			togglePanel(textures, LeftPane);
			return true;

		} else if (evt.key.keysym.sym == SDLK_F2) {

			// toggle the right (codebook) pane
			togglePanel(textures, RightPane);
			cs_open = true;
			editingBox = &tex_cs;
			editStr = "";
			cursor_pos = 0;
			for (int i = 0; i < 26; i++){
				editStr = editStr + substitution[i];
			}
			editMode = true;
			display_state.status = INPUT;
			clear_png(&tex_cs);
			render_text(&tex_cs, editStr, green, 'd', 75);
			update_texture(&tex_cs);
			return true;

		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			downArrow.pressed = false;
			size_t choices = display_state.jumps.size();
			if (choices > 0) {
				if (display_state.current_choice < choices - 1) {
					display_state.current_choice++;	
					clear_png(&tex_box_text);
					render_text(&tex_box_text, display_state.jump_names[display_state.current_choice], white, display_state.cipher);
					update_texture(&tex_box_text);
				}
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			upArrow.pressed = false;
			if (display_state.jumps.size() > 0) {
				if (display_state.current_choice > 0) {
					display_state.current_choice--;
				
					clear_png(&tex_box_text);
					render_text(&tex_box_text, display_state.jump_names[display_state.current_choice], white, display_state.cipher);
					update_texture(&tex_box_text);
				}
			}
			return true;
		} 
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {

		// convert motion to texture coordinate system
		float tex_x = 2.0f*(((float)evt.motion.x)/window_size.x)-1.0f;
		float tex_y = -2.0f*(((float)evt.motion.y)/window_size.y)+1.0f;

		checkForClick(textures, tex_x, tex_y);

		// only advance if click inside of dialogue
		if (display_state.status != INPUT &&
			display_state.status != WAIT_FOR_SOLVE &&
			tex_x >= tex_textbg.bounds[0] &&
			tex_x < tex_textbg.bounds[1] &&
			tex_y >= tex_textbg.bounds[2] &&
			tex_y < tex_textbg.bounds[3])
		{

			clear_png(&tex_box_text);
			advance_state(0);
			
		}

	} else if (evt.type == SDL_MOUSEMOTION) {
		// float tex_x = 2.0f*(((float)evt.motion.x)/window_size.x)-1.0f;
		// float tex_y = -2.0f*(((float)evt.motion.y)/window_size.y)+1.0f;

		// std::cout << tex_x << ", " << tex_y << std::endl;
		
	}


	return false;
}

void PlayMode::update(float elapsed) {

	// move creechur
	if (swap_creature->position.x < x_by_counter) {
		swap_creature->position.x += creature_speed * elapsed;
	}

	updateTextures(textures);

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

	glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

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

		glBindVertexArray(tex_box_text.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_box_text.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_box_text.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_box_text.count);
		GL_ERRORS();


		if (!textures[1]->visible)
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