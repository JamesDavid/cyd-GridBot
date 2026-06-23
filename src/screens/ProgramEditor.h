// GridBot — the shared block editor: control pad (left) + nested program list (right).
// Extracted from GameScreen so the SAME authoring UI is reused everywhere kids program:
// the campaign, Puzzle Race, and the CodeLab lessons. It owns the authoring state
// (selection, scroll, which body is edited) and edits a Program* the host owns; the host
// keeps the maze, the run/animation, and the chrome. handleTap() returns an Action so the
// host can react to RUN / train-a-brain / library save+load. (SPEC §10, §11.1)
#pragma once
#include <vector>
#include <string>
#include "app/Screen.h"
#include "hal/Display.h"
#include "ui/UI.h"
#include "game/Program.h"
#include "game/Profile.h"

namespace screens {

// Which blocks the pad offers + how to draw the centre avatar. Mirrors Profile::Unlocks
// but is passed explicitly so non-campaign hosts (Puzzle Race, lessons) can choose.
struct EditorConfig {
  bool jump = true, repeat = true, call = true, sense = true, func = false, neuro = false;
  bool library = false;  // show the Save>Lib / Load<Lib buttons (func tabs are separate, via `func`)
  int avatar = 0;
  gb::Facing facing = gb::EAST;
  const std::vector<uint8_t>* customChar = nullptr;  // optional custom sprite for the centre
  bool readOnly = false;  // lessons: show the program, ignore pad/edit taps
};

class ProgramEditor {
 public:
  enum class Action { NONE, RUN, NEURO_TRAIN, SAVE_LIB, LOAD_LIB };

  // Bind to a program + config. profile is optional (command histogram + library).
  void attach(gb::Program* prog, const EditorConfig& cfg, int par, gb::Profile* profile = nullptr);
  void rebind(gb::Program* prog);  // program replaced (library load): re-point to MAIN
  void setPar(int par) { _par = par; }
  void setFailNode(const gb::Node* n) { _failNode = n; }
  void resetSelection() { _selected = -1; _scroll = 0; _followTail = true; }

  void draw();              // control pad + program list, into the band
  void drawControlPad();
  void drawProgramList();
  Action handleTap(int x, int y);

  // Read-only render of a program body in the editor's exact row style (glyph + colour +
  // indented bracket + label). Lets lessons show code that LOOKS like the real editor
  // without embedding the whole pad. Returns the y just past the last row.
  static int drawReadOnlyList(LGFX& g, const gb::NodeList& list, int x, int y,
                              int depth = 0, uint16_t bracket = 0, int rowH = 16);

  int neuroIdx() const { return _neuroIdx; }      // valid after Action::NEURO_TRAIN
  int writtenCount() const { return _writtenCount; }
  gb::Program* program() const { return _prog; }
  gb::NodeList* editList() const { return _editList; }

 private:
  // flattened program-list row (rebuilt each draw). addSlot = synthetic "+ add inside/here";
  // trainSlot = the "train this brain" line under a NEURO node. Both have index = -1.
  struct Row { gb::Node* node; gb::NodeList* list; int index; int depth; uint16_t bracket; bool addSlot; bool trainSlot = false; };

  void flatten(gb::NodeList& list, int depth, std::vector<Row>& out, uint16_t bracket = 0);
  gb::NodeList* appendTarget();
  void appendNodeToTarget(const gb::Node& n);
  void appendCommand(gb::Cmd c);
  void deleteSelected();
  void moveSelected(int dir);   // reorder the selected line within its block (-1 up, +1 down)
  bool cornerUnlocked(int slot) const;  // 0=JUMP,1=REPEAT,2=CALL,3=SENSE
  ui::Rect funcTabRect(int i) const;
  int listTop() const;
  void handlePadTap(int x, int y);
  Action handleListTap(int x, int y);

  gb::Program* _prog = nullptr;
  gb::NodeList* _editList = nullptr;
  gb::Profile* _profile = nullptr;   // command histogram + library tabs (optional)
  EditorConfig _cfg;
  int _par = 1, _writtenCount = 0;
  int _selected = -1, _scroll = 0;
  bool _followTail = true;
  const gb::Node* _failNode = nullptr;
  int _neuroIdx = -1;
};

}  // namespace screens
