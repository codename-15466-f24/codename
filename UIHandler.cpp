#include "UIHandler.hpp"

#include <glm/glm.hpp>
#include "load_save_png.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <deque>
#include <iostream>


std::vector<TexStruct *> initializeTextures(std::vector<PanePosition> alignments, 
								std::vector<bool> visibilities,
								std::vector<std::function<void(std::vector<TexStruct *>, std::string)>> callbacks)
{
	std::vector<TexStruct *> textures;
	for (size_t i = 0; i < alignments.size(); i++)
	{
		TexStruct *t = new TexStruct(alignments[i]);
		t->visible = visibilities[i];
		t->onClick = callbacks[i];

		textures.push_back(t);
	}

	return textures;
}

void togglePanel(std::vector<TexStruct *> textures, PanePosition alignment)
{
	for (auto tex : textures)
	{
		if (tex->alignment == alignment)
		{
			tex->visible = !tex->visible;

		}
	}

}

void addTextures(std::vector<TexStruct *> textures, std::vector<std::string> paths, const TextureProgram *ui_texture_program)
{
	size_t path_index = 0;

	for (TexStruct *tex_ : textures)
	{
		assert(tex_);
		auto &tex = *tex_;
		tex.path = paths[path_index];
		 
		// from in-class example
		glGenTextures(1, &tex.tex); 
		{
			//load texture data as RGBA from a file:
			std::vector< glm::u8vec4 > data;
			glm::uvec2 size;

			load_png(data_path(paths[path_index]), &size, &data, LowerLeftOrigin);

			tex.sizeX = (float)size.x;
			tex.sizeY = (float)size.y;

			glBindTexture(GL_TEXTURE_2D, tex.tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);

			// texture, level, color scheme, width height, border
			// add border bc sometimes it can get weird in linear sampling, more control over interpolation
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			// maybe some aliasing, sampling lower detail than the texture
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

			glBindTexture(GL_TEXTURE_2D, 0);
		};

		glGenBuffers(1, &tex.tristrip_buffer);
		{
			glGenVertexArrays(1, &tex.tristrip_buffer_for_texture_program_vao); 
			// make buffer of pos tex vertices
			glBindVertexArray(tex.tristrip_buffer_for_texture_program_vao);
			glBindBuffer(GL_ARRAY_BUFFER, tex.tristrip_buffer);

			//size, type, normalize, stride, offset <-- recall from reading
			// these are postex vertices
			glVertexAttribPointer(ui_texture_program->Position_vec4,
				3,
				GL_FLOAT,
				GL_FALSE,
				sizeof(PosTexVertex),
				(GLbyte*)0 + offsetof(PosTexVertex, Position)
			);

			glEnableVertexAttribArray(ui_texture_program->Position_vec4);

			//size, type, normalize, stride, offset <-- recall from reading
			// these are postex vertices
			glVertexAttribPointer(ui_texture_program->TexCoord_vec2,
				2,
				GL_FLOAT,
				GL_FALSE,
				sizeof(PosTexVertex),
				(GLbyte*)0 + offsetof(PosTexVertex, TexCoord)
			);

			glEnableVertexAttribArray(ui_texture_program->TexCoord_vec2);


			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		} 
		path_index++;
	}

}

void updateTextures(std::vector<TexStruct *> textures)
{
	auto texUpdate = [&] (TexStruct *tex, float offset){
	
		// don't do this if back face culling
		std::vector<PosTexVertex> verts;

		float padding = 0.001f;
		// is right aligned
		if (tex->alignment == RightPane)
		{
			tex->bounds = {1.0f-tex->relativeSizeX, 
							1.0f, 
							1.0f - tex->relativeSizeY - (offset*tex->relativeSizeY) - (offset*padding),
							1.0f - (offset*tex->relativeSizeY) - (offset*padding)};
			
		} else if (tex->alignment == LeftPane) {
			tex->bounds = { -1.0f,
							-1.0f+tex->relativeSizeX,
							1.0f - tex->relativeSizeY - (offset*tex->relativeSizeY) - (offset*padding),
							1.0f - (offset*tex->relativeSizeY) - (offset*padding)

			};

		} else if(tex->alignment == TopMiddlePane || tex->alignment == TopMiddlePaneSelected)
		{
			tex->bounds = {
							10*(offset*padding) + (offset*tex->relativeSizeX)-(2.0f*tex->relativeSizeX),
							10*(offset*padding) + (offset*tex->relativeSizeX)-tex->relativeSizeX,
							0.95f - tex->relativeSizeY,
							0.95f
						};

		}
		else if (tex->alignment == MiddlePane || tex->alignment == MiddlePaneSelected)
		{
			tex->bounds = { -tex->relativeSizeX/2.0f,
				tex->relativeSizeX/2.0f,
				-tex->relativeSizeY/4.0f-2.0f*(offset*tex->relativeSizeY),
				3*tex->relativeSizeY/4.0f-2.0f*(offset*tex->relativeSizeY)};
		} else if (tex->alignment == MiddlePaneBG)
		{
			tex->bounds = { -tex->relativeSizeX/2.0f,
				tex->relativeSizeX/2.0f,
				-tex->relativeSizeY/4.0f,
				3*tex->relativeSizeY/4.0f};
		} 
		else
		{

			// top middle plane bg
			tex->bounds = {
				-tex->relativeSizeX/2.0f,
				tex->relativeSizeX/2.0f,
				1.0f - tex->relativeSizeY,
				1.0
			};

		}

		verts.emplace_back(PosTexVertex{
			.Position = glm::vec3(tex->bounds[0], tex->bounds[2], 0.0f),
			.TexCoord = glm::vec2(0.0f, 0.0f),
			});

			verts.emplace_back(PosTexVertex{
				.Position = glm::vec3(tex->bounds[0], tex->bounds[3], 0.0f),
				.TexCoord = glm::vec2(0.0f, 1.0f),
			});

			verts.emplace_back(PosTexVertex{
				.Position = glm::vec3(tex->bounds[1], tex->bounds[2], 0.0f),
				.TexCoord = glm::vec2(1.0f, 0.0f),
			});

			verts.emplace_back(PosTexVertex{
				.Position = glm::vec3(tex->bounds[1], tex->bounds[3], 0.0f),
				.TexCoord = glm::vec2(1.0f, 1.0f),
			});


		glBindBuffer(GL_ARRAY_BUFFER, tex->tristrip_buffer);

		// tells us update:  GL_STREAM_DRAW, stream = update once a frame, draw = what we're gonna do
		// could read, draw copy etc.
		// worst case, run slower, telling about gpu memory, fast for static or stream from memory
		glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(verts[0]), verts.data(), GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		tex->count = (GLuint)verts.size();
	};

	float left_offset = 0.0f;
	float right_offset = 0.0f;
	float top_middle_offset = 0.0f;
	float top_middle_selected_offset = 0.0f;
	float middle_offset = 0.0f;

	for (auto tex : textures)
	{
		if (tex->visible)
		{
			if (tex->alignment == RightPane)
			{
				texUpdate(tex, right_offset);
				right_offset++;
			} else if (tex->alignment == LeftPane) {
				texUpdate(tex, left_offset);
				left_offset++;
			} else if (tex->alignment == TopMiddlePane) {
				texUpdate(tex, top_middle_offset);
				top_middle_offset++;
			} else if (tex->alignment == TopMiddlePaneSelected) {
				texUpdate(tex, top_middle_selected_offset);
				top_middle_selected_offset++;
			}
		} 

	if (tex->alignment == MiddlePane)
		{
			texUpdate(tex, middle_offset);
			middle_offset++;

		} else {
			texUpdate(tex, 0.0f);
		}
	}

}

void drawTextures(std::vector<TexStruct *> textures, const TextureProgram *ui_texture_program)
{
	// from in-class example
	auto drawTex = [&](TexStruct *tex) {

		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(ui_texture_program->program);
		glBindVertexArray(tex->tristrip_buffer_for_texture_program_vao);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->tex);
		// number, transposed or not
		glUniformMatrix4fv(ui_texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex->CLIP_FROM_LOCAL));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex->count);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_BLEND);
	};

	for (auto tex : textures)
	{
		if (tex->visible)
		{
			drawTex(tex);
		}
	}
}

void rescaleTextures(std::vector<TexStruct *> textures, glm::vec2 window_size)
{
	for (auto tex_ : textures)
	{
		assert(tex_);
		auto &tex = *tex_;

		tex.relativeSizeX = window_size.x > 0.0f ? tex.sizeX/window_size.x : tex.relativeSizeX;
		tex.relativeSizeY = window_size.y > 0.0f ? tex.sizeY/window_size.y : tex.relativeSizeY;
	
	}
}
	
bool checkForClick(std::vector<TexStruct *> textures, float x, float y)
{
	bool isLocked = false;

	for (auto tex_ : textures)
	{
		assert(tex_);
		auto &tex = *tex_;


		// don't allow clicking on invisible textures
		if (tex.visible)
		{

			if (x >= tex.bounds[0] &&
				x < tex.bounds[1] &&
				y > tex.bounds[2] &&
				y <= tex.bounds[3])
			{
				tex.onClick(textures, tex.path);
				break;
			}

		}
	}


	for (auto tex : textures)
	{
		if (tex->visible && tex->path == "mini_puzzle_panel.png")
		{
			isLocked = true;
		}
	}

	return isLocked;
}

