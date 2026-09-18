/*
 * $Id: dungeonview.cpp 3071 2014-07-26 18:01:08Z darren_janeczek $
 */

#include "dungeonview.h"
#include "imagemgr.h"
#include "screen.h"
#include "tileanim.h"
#include "u4.h"
#include "error.h"
#ifdef ZU4_IOS
#include "mobile_dungeon_sight.h"
#include "zu4_ios_ui.h"
#endif

DungeonView::DungeonView(int x, int y, int columns, int rows) : TileView(x, y, rows, columns)
, screen3dDungeonViewEnabled(true)
{
}

DungeonView * DungeonView::instance(NULL);
DungeonView * DungeonView::getInstance()
{
	if (!instance) 	{
		instance = new DungeonView(BORDER_WIDTH, BORDER_HEIGHT, VIEWPORT_W, VIEWPORT_H);
	}
	return instance;
}

void DungeonView::display(Context * c, TileView *view)
{
	int x,y;

    /* 1st-person perspective */
    if (screen3dDungeonViewEnabled) {
        //Note: This shouldn't go above 4, unless we check opaque tiles each step of the way.
        const int farthest_non_wall_tile_visibility = 4;

        std::vector<MapTile> tiles;

        screenEraseMapArea();
        if (c->party->getTorchDuration() > 0) {
            for (y = 3; y >= 0; y--) {
                DungeonGraphicType type;

                //FIXME: Maybe this should be in a loop
				tiles = getTiles(y, -1);
                type = tilesToGraphic(tiles);
				drawWall(-1, y, (Direction)c->saveGame->orientation, type);

				tiles = getTiles(y, 1);
                type = tilesToGraphic(tiles);
                drawWall(1, y, (Direction)c->saveGame->orientation, type);

                tiles = getTiles(y, 0);
                type = tilesToGraphic(tiles);
                drawWall(0, y, (Direction)c->saveGame->orientation, type);

                //This only checks that the tile at y==3 is opaque
                if (y == 3 && !tiles.front().getTileType()->isOpaque())
               	{
               		for (int y_obj = farthest_non_wall_tile_visibility; y_obj > y; y_obj--)
               		{
                        std::vector<MapTile> distant_tiles = getTiles(y_obj     , 0);
               		DungeonGraphicType distant_type = tilesToGraphic(distant_tiles);

					if ((distant_type == DNGGRAPHIC_DNGTILE) || (distant_type == DNGGRAPHIC_BASETILE))
						drawTile(c->location->map->tileset->get(distant_tiles.front().getId()),0, y_obj, Direction(c->saveGame->orientation));
               		}
               	}
				if ((type == DNGGRAPHIC_DNGTILE) || (type == DNGGRAPHIC_BASETILE))
					drawTile(c->location->map->tileset->get(tiles.front().getId()), 0, y, Direction(c->saveGame->orientation));
            }
        }
    }

    /* 3rd-person perspective */
    else {
        std::vector<MapTile> tiles;

        static MapTile black = c->location->map->tileset->getByName("black")->getId();
        static MapTile avatar = c->location->map->tileset->getByName("avatar")->getId();
#ifdef ZU4_IOS
        static MapTile wall = c->location->map->tileset->getByName("brick_wall")->getId();
#endif

#ifdef ZU4_IOS
        // The mobile overhead presentation is a fixed, north-up floor plan.
        // Dungeon floors are 8x8 while the classic viewport is 11x11; drawing
        // the entire floor once avoids repeating wrapped cells at the edges.
        int mapWidth = (int)c->location->map->width;
        int mapHeight = (int)c->location->map->height;
        int originX = std::max(0, (VIEWPORT_W - mapWidth) / 2);
        int originY = std::max(0, (VIEWPORT_H - mapHeight) / 2);

        // Augment the overhead view with the party's current cardinal line of
        // sight and remember every legitimately seen cell. Dungeon corridors
        // are orthogonal and the maps wrap, so sight crosses a floor seam into
        // the mechanically adjacent cell at the opposite edge. Each floor cell
        // is still drawn exactly once in its fixed north-up position.
        // The blocking cell itself is visible; cells beyond it are not.  Use
        // the renderer's dungeon graphic classification in addition to tile
        // opacity because ordinary dungeon doors are intentionally not marked
        // opaque in the shared base tileset.
        std::vector<unsigned char> sightBlockers(
            (std::size_t)mapWidth * mapHeight, 0);
        for (int sightY = 0; sightY < mapHeight; ++sightY) {
            for (int sightX = 0; sightX < mapWidth; ++sightX) {
                Coords probe = {sightX, sightY, c->location->coords.z};
                bool focus = false;
                std::vector<MapTile> sightTiles = c->location->tilesAt(probe, focus);
                DungeonGraphicType graphic = tilesToGraphic(sightTiles);
                sightBlockers[(std::size_t)sightY * mapWidth + sightX] =
                    sightTiles.front().getTileType()->isOpaque() ||
                    graphic == DNGGRAPHIC_WALL || graphic == DNGGRAPHIC_DOOR;
            }
        }
        std::vector<unsigned char> currentVisibility = mobileDungeonCardinalSight(
            mapWidth, mapHeight, c->location->coords.x, c->location->coords.y,
            sightBlockers);

        for (y = 0; y < VIEWPORT_H; y++) {
            for (x = 0; x < VIEWPORT_W; x++) {
                int mapX = x - originX;
                int mapY = y - originY;
                bool inside = mapX >= 0 && mapY >= 0 &&
                    mapX < mapWidth && mapY < mapHeight;
                if (c->party->getTorchDuration() <= 0 || !inside) {
                    view->drawTile(black, false, x, y);
                    continue;
                }

                Coords coords = {mapX, mapY, c->location->coords.z};
                if (zu4_coords_equal(coords, c->location->coords)) {
                    view->drawTile(avatar, false, x, y);
                    continue;
                }
                bool explored = zu4_mobile_dungeon_cell_revealed(
                    (int)c->location->map->id, mapX, mapY, coords.z);
                if (!explored) {
                    if (!currentVisibility[(std::size_t)mapY * mapWidth + mapX]) {
                        view->drawTile(black, false, x, y);
                        continue;
                    }
                    // A seen wall or door should remain part of the player's
                    // mental map after it leaves the live sight ray.
                    zu4_mobile_dungeon_remember_cell(
                        (int)c->location->map->id, mapX, mapY, coords.z);
                }

                bool focus = false;
                tiles = c->location->tilesAt(coords, focus);
                Dungeon *dungeon = static_cast<Dungeon *>(c->location->map);
                if (dungeon->tokenAt(coords) == DUNGEON_SECRET_DOOR)
                    view->drawTile(wall, false, x, y);
                else
                    view->drawTile(tiles, false, x, y);
            }
        }
#else
        for (y = 0; y < VIEWPORT_H; y++) {
            for (x = 0; x < VIEWPORT_W; x++) {
                int fwd = (VIEWPORT_H / 2) - y;
                int side = x - (VIEWPORT_W / 2);
                tiles = getTiles(fwd, side);

				/* Only show blackness if there is no light */
				if (c->party->getTorchDuration() <= 0)
					view->drawTile(black, false, x, y);
				else if (x == VIEWPORT_W/2 && y == VIEWPORT_H/2)
					view->drawTile(avatar, false, x, y);
				else
					view->drawTile(tiles, false, x, y);
            }
        }
#endif
    }
}

void DungeonView::drawInDungeon(Tile *tile, int x_offset, int distance, Direction orientation, bool tiledWall) {
    Image *scaled;

    const static int nscale_vga[] = { 12, 8, 4, 2, 1};
    const static int nscale_ega[] = { 8, 4, 2, 1, 0};

    const int lscale_vga[] = { 22, 18, 10, 4, 1};
    const int lscale_ega[] = { 22, 14, 6, 3, 1};

    const int * lscale;
    const int * nscale;
    int offset_multiplier = 0;
    int offset_adj = 0;
    if (settings.videoType) // Not EGA
    {
    	lscale = & lscale_vga[0];
    	nscale = & nscale_vga[0];
    	offset_multiplier = 1;
    	offset_adj = 2;
    }
    else
    {
    	lscale = & lscale_ega[0];
    	nscale = & nscale_ega[0];
    	offset_adj = 1;
    	offset_multiplier = 4;
    }

    const int *dscale = tiledWall ? lscale : nscale;

    //Clear scratchpad and set a background color
    zu4_img_fill(animated, 0, 0, animated->w, animated->h, 14, 15, 16, 255);
    //Put tile on animated scratchpad
    if (tile->getAnim()) {
        MapTile mt = tile->getId();
        tile->getAnim()->draw(animated, tile, mt, orientation);
    }
    else {
        zu4_img_draw_on(animated, tile->getImage(), 0, 0);
    }

    /* scale is based on distance; 1 means half size, 2 regular, 4 means scale by 2x, etc. */
    if (dscale[distance] == 0)
		return;
    else if (dscale[distance] == 1)
        scaled = zu4_img_scaledown(animated, 2);
    else
        scaled = zu4_img_scaleup(animated, dscale[distance] / 2);

    if (tiledWall) {
    	int i_x = ((VIEWPORT_W * tileWidth  / 2) + this->x) - (scaled->w / 2);
    	int i_y = ((VIEWPORT_H * tileHeight / 2) + this->y) - (scaled->h / 2);
    	int f_x = i_x + scaled->w;
    	int f_y = i_y + scaled->h;
    	int d_x = animated->w;
    	int d_y = animated->h;

    	for (int x = i_x; x < f_x; x+=d_x)
    		for (int y = i_y; y < f_y; y+=d_y)
    			zu4_img_draw_subrect_on(this->screen, animated,
    					x, y, 0, 0, f_x - x, f_y - y);
    }
    else {
    	int y_offset = std::max(0,(dscale[distance] - offset_adj) * offset_multiplier);
    	int x = ((VIEWPORT_W * tileWidth / 2) + this->x) - (scaled->w / 2);
    	int y = ((VIEWPORT_H * tileHeight / 2) + this->y + y_offset) - (scaled->h / 8);

		zu4_img_draw_subrect_on(this->screen, scaled,
								x, y, 0, 0, scaled->w, scaled->h);
    }

    zu4_img_free(scaled);
}

int DungeonView::graphicIndex(int xoffset, int distance, Direction orientation, DungeonGraphicType type) {
    int index;

    index = 0;

    if (type == DNGGRAPHIC_LADDERUP && xoffset == 0)
        return 48 +
        (distance * 2) +
        (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH) ? 1 : 0);

    if (type == DNGGRAPHIC_LADDERDOWN && xoffset == 0)
        return 56 +
        (distance * 2) +
        (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH) ? 1 : 0);

    if (type == DNGGRAPHIC_LADDERUPDOWN && xoffset == 0)
        return 64 +
        (distance * 2) +
        (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH) ? 1 : 0);

    /* FIXME */
    if (type != DNGGRAPHIC_WALL && type != DNGGRAPHIC_DOOR)
        return -1;

    if (type == DNGGRAPHIC_DOOR)
        index += 24;

    index += (xoffset + 1) * 2;

    index += distance * 6;

    if (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH))
        index++;

    return index;
}

void DungeonView::drawTile(Tile *tile, int x_offset, int distance, Direction orientation) {
    // Draw the tile to the screen
	DungeonViewer.drawInDungeon(tile, x_offset, distance, orientation, tile->isTiledInDungeon());
}

std::vector<MapTile> DungeonView::getTiles(int fwd, int side) {
    Coords coords = c->location->coords;

    switch (c->saveGame->orientation) {
    case DIR_WEST:
        coords.x -= fwd;
        coords.y -= side;
        break;

    case DIR_NORTH:
        coords.x += side;
        coords.y -= fwd;
        break;

    case DIR_EAST:
        coords.x += fwd;
        coords.y += side;
        break;

    case DIR_SOUTH:
        coords.x -= side;
        coords.y += fwd;
        break;

    case DIR_ADVANCE:
    case DIR_RETREAT:
    default:
        zu4_assert(0, "Invalid dungeon orientation");
    }

    // Wrap the coordinates if necessary
    wrap(&coords, c->location->map);

    bool focus;
    Coords coords2(coords);
    return c->location->tilesAt(coords2, focus);
}

DungeonGraphicType DungeonView::tilesToGraphic(const std::vector<MapTile> &tiles) {
    MapTile tile = tiles.front();

    static const MapTile corridor = c->location->map->tileset->getByName("brick_floor")->getId();
    static const MapTile up_ladder = c->location->map->tileset->getByName("up_ladder")->getId();
    static const MapTile down_ladder = c->location->map->tileset->getByName("down_ladder")->getId();
    static const MapTile updown_ladder = c->location->map->tileset->getByName("up_down_ladder")->getId();

    /*
     * check if the dungeon tile has an annotation or object on top
     * (always displayed as a tile, unless a ladder)
     */
    if (tiles.size() > 1) {
        if (tile.id == up_ladder.id)
            return DNGGRAPHIC_LADDERUP;
        else if (tile.id == down_ladder.id)
            return DNGGRAPHIC_LADDERDOWN;
        else if (tile.id == updown_ladder.id)
            return DNGGRAPHIC_LADDERUPDOWN;
        else if (tile.id == corridor.id)
            return DNGGRAPHIC_NONE;
        else
            return DNGGRAPHIC_BASETILE;
    }

    /*
     * if not an annotation or object, then the tile is a dungeon
     * token
     */
    Dungeon *dungeon = dynamic_cast<Dungeon *>(c->location->map);
    DungeonToken token = dungeon->tokenForTile(tile);

    switch (token) {
    case DUNGEON_TRAP:
    case DUNGEON_CORRIDOR:
        return DNGGRAPHIC_NONE;
    case DUNGEON_WALL:
    case DUNGEON_SECRET_DOOR:
        return DNGGRAPHIC_WALL;
    case DUNGEON_ROOM:
    case DUNGEON_DOOR:
        return DNGGRAPHIC_DOOR;
    case DUNGEON_LADDER_UP:
        return DNGGRAPHIC_LADDERUP;
    case DUNGEON_LADDER_DOWN:
        return DNGGRAPHIC_LADDERDOWN;
    case DUNGEON_LADDER_UPDOWN:
        return DNGGRAPHIC_LADDERUPDOWN;

    default:
        return DNGGRAPHIC_DNGTILE;
    }
}

const struct {
    const char *subimage;
    int ega_x2, ega_y2;
    int vga_x2, vga_y2;
    const char *subimage2;
} dngGraphicInfo[] = {
    { "dung0_lft_ew" },
    { "dung0_lft_ns" },
    { "dung0_mid_ew" },
    { "dung0_mid_ns" },
    { "dung0_rgt_ew" },
    { "dung0_rgt_ns" },

    { "dung1_lft_ew", 0, 32, 0, 8, "dung1_xxx_ew" },
    { "dung1_lft_ns", 0, 32, 0, 8, "dung1_xxx_ns" },
    { "dung1_mid_ew" },
    { "dung1_mid_ns" },
    { "dung1_rgt_ew", 144, 32, 160, 8, "dung1_xxx_ew" },
    { "dung1_rgt_ns", 144, 32, 160, 8, "dung1_xxx_ns" },

    { "dung2_lft_ew", 0, 64, 0, 48, "dung2_xxx_ew" },
    { "dung2_lft_ns", 0, 64, 0, 48, "dung2_xxx_ns" },
    { "dung2_mid_ew" },
    { "dung2_mid_ns" },
    { "dung2_rgt_ew", 112, 64, 128, 48, "dung2_xxx_ew" },
    { "dung2_rgt_ns", 112, 64, 128, 48, "dung2_xxx_ns" },

    { "dung3_lft_ew", 0, 80, 48, 72, "dung3_xxx_ew" },
    { "dung3_lft_ns", 0, 80, 48, 72, "dung3_xxx_ns" },
    { "dung3_mid_ew" },
    { "dung3_mid_ns" },
    { "dung3_rgt_ew", 96, 80, 104, 72, "dung3_xxx_ew" },
    { "dung3_rgt_ns", 96, 80, 104, 72, "dung3_xxx_ns" },

    { "dung0_lft_ew_door" },
    { "dung0_lft_ns_door" },
    { "dung0_mid_ew_door" },
    { "dung0_mid_ns_door" },
    { "dung0_rgt_ew_door" },
    { "dung0_rgt_ns_door" },

    { "dung1_lft_ew_door", 0, 32, 0, 8, "dung1_xxx_ew" },
    { "dung1_lft_ns_door", 0, 32, 0, 8, "dung1_xxx_ns" },
    { "dung1_mid_ew_door" },
    { "dung1_mid_ns_door" },
    { "dung1_rgt_ew_door", 144, 32, 160, 8, "dung1_xxx_ew" },
    { "dung1_rgt_ns_door", 144, 32, 160, 8, "dung1_xxx_ns" },

    { "dung2_lft_ew_door", 0, 64, 0, 48, "dung2_xxx_ew" },
    { "dung2_lft_ns_door", 0, 64, 0, 48, "dung2_xxx_ns" },
    { "dung2_mid_ew_door" },
    { "dung2_mid_ns_door" },
    { "dung2_rgt_ew_door", 112, 64, 128, 48, "dung2_xxx_ew" },
    { "dung2_rgt_ns_door", 112, 64, 128, 48, "dung2_xxx_ns" },

    { "dung3_lft_ew_door", 0, 80, 48, 72, "dung3_xxx_ew" },
    { "dung3_lft_ns_door", 0, 80, 48, 72, "dung3_xxx_ns" },
    { "dung3_mid_ew_door" },
    { "dung3_mid_ns_door" },
    { "dung3_rgt_ew_door", 96, 80, 104, 72, "dung3_xxx_ew" },
    { "dung3_rgt_ns_door", 96, 80, 104, 72, "dung3_xxx_ns" },

    { "dung0_ladderup" },
    { "dung0_ladderup_side" },
    { "dung1_ladderup" },
    { "dung1_ladderup_side" },
    { "dung2_ladderup" },
    { "dung2_ladderup_side" },
    { "dung3_ladderup" },
    { "dung3_ladderup_side" },

    { "dung0_ladderdown" },
    { "dung0_ladderdown_side" },
    { "dung1_ladderdown" },
    { "dung1_ladderdown_side" },
    { "dung2_ladderdown" },
    { "dung2_ladderdown_side" },
    { "dung3_ladderdown" },
    { "dung3_ladderdown_side" },

    { "dung0_ladderupdown" },
    { "dung0_ladderupdown_side" },
    { "dung1_ladderupdown" },
    { "dung1_ladderupdown_side" },
    { "dung2_ladderupdown" },
    { "dung2_ladderupdown_side" },
    { "dung3_ladderupdown" },
    { "dung3_ladderupdown_side" },
};

void DungeonView::drawWall(int xoffset, int distance, Direction orientation, DungeonGraphicType type) {
    int index;

    index = graphicIndex(xoffset, distance, orientation, type);
    if (index == -1 || distance >= 4)
        return;

    int x = 0, y = 0;
    SubImage *subimage = imageMgr->getSubImage(dngGraphicInfo[index].subimage);
    if (subimage) {
        x = subimage->x;
        y = subimage->y;
    }

    screenDrawImage(dngGraphicInfo[index].subimage, (BORDER_WIDTH + x),
                    (BORDER_HEIGHT + y));

    if (dngGraphicInfo[index].subimage2 != NULL) {
        // FIXME: subimage2 is a horrible hack, needs to be cleaned up
        if (!settings.videoType) // EGA
            screenDrawImage(dngGraphicInfo[index].subimage2,
                            (8 + dngGraphicInfo[index].ega_x2),
                            (8 + dngGraphicInfo[index].ega_y2));
        else
            screenDrawImage(dngGraphicInfo[index].subimage2,
                            (8 + dngGraphicInfo[index].vga_x2),
                            (8 + dngGraphicInfo[index].vga_y2));
    }
}
