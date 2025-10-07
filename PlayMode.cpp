//Chat GPT help debug
//reference: https://github.com/flowerflora/MultiplayerGame
#include "PlayMode.hpp"
#include "Game.hpp"
#include "LitColorTextureProgram.hpp"
#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

// ----- Load Ocean meshes and scene -----	
GLuint Ocean_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > Ocean_meshes(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("ocean.pnct"));
    Ocean_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});
static constexpr float ARENA_TO_SCENE = 7.5f;

Load< Scene > Ocean_scene(LoadTagDefault, []() -> Scene const * {
    return new Scene(data_path("ocean.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
        Mesh const &mesh = Ocean_meshes->lookup(mesh_name);

        scene.drawables.emplace_back(transform);
        Scene::Drawable &drawable = scene.drawables.back();

        drawable.pipeline = lit_color_texture_program_pipeline;
        drawable.pipeline.vao = Ocean_meshes_for_lit_color_texture_program;
        drawable.pipeline.type = mesh.type;
        drawable.pipeline.start = mesh.start;
        drawable.pipeline.count = mesh.count;
    });
});


GLuint yyq_meshes_for_lit_color_texture_program = 1;
Load< MeshBuffer > yyq_meshes(LoadTagDefault, []() -> MeshBuffer const * {
    MeshBuffer const *ret = new MeshBuffer(data_path("yyq.pnct"));
    yyq_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});



PlayMode::PlayMode(Client &client_) : client(client_) {
	scene = *Ocean_scene;

	//create a camera
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	camera = &scene.cameras.back();
	camera->transform->position = glm::vec3(0,0,20); 

}

void PlayMode::sync_players_to_scene() {
    while (player_transforms.size() < game.players.size()) {
       
        Mesh const &mesh = yyq_meshes->lookup("player1");

       
        Scene::Transform *t = new Scene::Transform();
        scene.drawables.emplace_back(t);
        Scene::Drawable &dr = scene.drawables.back();

        dr.pipeline = lit_color_texture_program_pipeline;
        dr.pipeline.vao   = yyq_meshes_for_lit_color_texture_program;
        dr.pipeline.type  = mesh.type;
        dr.pipeline.start = mesh.start;
        dr.pipeline.count = mesh.count;

        player_transforms.emplace_back(t);
    }

    
    while (player_transforms.size() > game.players.size()) {
        Scene::Transform *t = player_transforms.back();
        for (auto it = scene.drawables.begin(); it != scene.drawables.end(); ) {
            if (it->transform == t) it = scene.drawables.erase(it);
            else ++it;
        }
        delete t;
        player_transforms.pop_back();
    }

    size_t i = 0;
    for (auto const &p : game.players) {
        Scene::Transform *t = player_transforms[i];
        t->position = glm::vec3(p.position.x * ARENA_TO_SCENE, p.position.y * ARENA_TO_SCENE, 1.0f + 0.001f * float(i));
        {
		bool local_is_turning = (controls.left.pressed || controls.right.pressed);

		glm::vec2 v = p.velocity;
		float angle;


		if (i == 0 && local_is_turning) {
			angle = player_angle;
		} else if (glm::length(v) > 1e-3f) {
			angle = std::atan2(v.y, v.x);
		} else {
			angle = (i == 0 ? player_angle : 0.0f); 
		}

		angle += glm::pi<float>();

		t->rotation = glm::angleAxis(angle, glm::vec3(0,0,1));
    	}        
        t->scale    = glm::vec3(1.0f);     
        ++i;
    }
}

void PlayMode::game_over() {
    if (game.players.empty()) return;

    Player &me = *game.players.begin(); // local player is first
    if (me.hp <= 0 && me.alive) {
        me.alive = false;
        me.velocity = glm::vec2(0.0f);
    }
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.key == SDLK_A) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_R) {
    if (!evt.key.repeat) {
        if (!game.players.empty()) {
            Player &me = *game.players.begin(); 
            if (!me.alive) {
                me.alive = true;
                me.hp = 10;
                me.velocity = glm::vec2(0.0f);
                me.pp_cd = me.wall_cd = 0.0f;
                me.position = 0.5f * (Game::ArenaMin + Game::ArenaMax);
            }
        }
    }
    return true;
}	
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	const float turn_speed = 2.5f;   
	const float dash_speed = 10.0f;  

	
	// rotation
	if (controls.left.pressed) {
		player_angle += turn_speed * elapsed;   // left rotation
	}
	if (controls.right.pressed) {
		player_angle -= turn_speed * elapsed;   // right rotation
	}

	// space dash
	if (controls.jump.pressed) {
		glm::vec2 forward = glm::vec2(std::cos(player_angle), std::sin(player_angle));
		if (!game.players.empty()) {
			Player &me = *game.players.begin();
		}
	}
	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);
	sync_players_to_scene();
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//draw camera
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	camera->transform->position = glm::vec3(0,0,20); 

	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);
	
	
	//figure out view transform to center the arena:
    float aspect = float(drawable_size.x) / float(drawable_size.y);
    float scale = std::min(
        2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
        2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
    );
    glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);
    glm::mat4 world_to_clip = glm::mat4(
        scale / aspect, 0.0f, 0.0f, offset.x,
        0.0f, scale, 0.0f, offset.y,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    {
        DrawLines lines(world_to_clip);
        auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
            lines.draw_text(text,
                glm::vec3(at.x, at.y, 0.0f),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0x00,0x00,0x00,0x00));
            float ofs = (1.0f / scale) / drawable_size.y;
            lines.draw_text(text,
                glm::vec3(at.x + ofs, at.y + ofs, 0.0f),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0xff,0xff,0xff,0x00));
        };
		//GPT help debug
		for (auto const &p : game.players) {
			glm::vec2 name_pos = p.position + glm::vec2(0.0f, 0.14f);
            glm::vec2 hp_pos   = p.position + glm::vec2(0.0f, 0.06f);
            // draw name slightly above the player:
            draw_text(name_pos, p.name, 0.09f);
            // draw HP / death message per player:
            if (p.alive) {
                draw_text(hp_pos, "HP: " + std::to_string(p.hp), 0.08f);
            } else {
                draw_text(hp_pos, "GAME OVER (Press R)", 0.08f);
            }
        }
    }
    GL_ERRORS();
}
