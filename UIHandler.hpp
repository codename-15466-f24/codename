#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include "TextureProgram.hpp"

// textures
struct PosTexVertex {
	glm::vec3 Position;
	glm::vec2 TexCoord;
};

// potentially expand to up or down?
enum PanePosition {
	LeftPane,
	RightPane
};

struct TexStruct {
	TexStruct(PanePosition align)
	{
		alignment = align;
	};
	~TexStruct();

	GLuint tex = 0;
	GLuint tristrip_buffer = 0; 
	GLuint tristrip_buffer_for_texture_program_vao  = 0; // vao, describes how to attach tristrip

	GLsizei count = 0; // how many things are there
	glm::mat4 CLIP_FROM_LOCAL = glm::mat4(1.0f);

	// size relative to the window
	float relativeSizeX = 0;
	float relativeSizeY = 0;

	// size in pixels
	float sizeX = 0;
	float sizeY = 0;
	
	// stick to right or left side of screen
	PanePosition alignment = LeftPane;
	bool visible = false;

};

std::vector<TexStruct *> initializeTextures(std::vector<PanePosition> alignments);

void togglePanel(std::vector<TexStruct *> textures, PanePosition alignment);

void addTextures(std::vector<TexStruct *> textures, std::vector<std::string> paths, const TextureProgram *ui_texture_program);

void updateTextures(std::vector<TexStruct *> textures);

void drawTextures(std::vector<TexStruct *> textures, const TextureProgram *ui_texture_program);

void rescaleTextures(std::vector<TexStruct *> textures, glm::vec2 window_size);

	

