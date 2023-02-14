#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

using std::string_literals::operator""s;

static const auto alpha = "abcdefghijklmnopqrstuvwxyz"s;
static const auto ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"s;

namespace rotors {
static const auto etw_alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"s;
static const auto etw_qwert = "QWERTYUIOPASDFGHJKLZXCVBNM"s;
static const auto i = "EKMFLGDQVZNTOWYHXUSPAIBRCJ"s;
static const auto ii = "AJDKSIRUXBLHWTMCQGZNPYFVOE"s;
static const auto iii = "BDFHJLCPRTXVZNYEIWGAKMUSQO"s;
static const auto iv = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"s;
static const auto v = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"s;
static const auto ukw_b = "YRUHQSLDPXNGOKMIEBFZCWVJAT"s;
} // namespace rotors

template <typename T>
class Reverse {
  T &iter_;

public:
  explicit Reverse(T &iter)
      : iter_{iter} {}

  auto begin() const { return std::rbegin(iter_); }
  auto end() const { return std::rend(iter_); }
};

template <typename T>
auto make_reverse(T &iter) {
  return Reverse<T>(iter);
}

auto make_notches(uint32_t len, std::initializer_list<uint32_t> vals) {
  std::vector<uint32_t> ret(len, 0u);
  for (uint32_t val : vals) {
    ret[val] = 1u;
  }
  return ret;
}

/* A Rotor consists of two "Shuffle"s, which are simply vectors of ints. These
 * are used as maps from the input int to output int, which makes the ciphering
 * operation as simple as indexing into each Rotor with the previous Rotor's
 * outputs, much like simple connected wires through the machine.
 * The Rotor has two Shuffles because it is easier to use twice the memory and
 * have a simple backward shuffle, created on construction, than it is to try
 * to work out the inverse mapping each time the Rotor is run backwards.
 * Additionally there is the "notches" array, used to determine when to rotate
 * the Rotors, as well as variables for the current position of the rotor and
 * its length. */
class Rotor {
  using Shuffle = std::vector<uint32_t>;
  Shuffle shuffle_fwd_;
  Shuffle shuffle_bck_;
  Shuffle notches_;
  Shuffle::size_type len_;
  Shuffle::size_type pos_;
  constexpr static auto a_ = 'A';

public:
  // Delegate to some member function that sorts out the shuffles
  // Move the iterator copy and a_ subtraction into lambda or helper function
  // that can be moved-from
  Rotor()
      : shuffle_fwd_{ALPHA.begin(), ALPHA.end()}
      , shuffle_bck_{ALPHA.begin(), ALPHA.end()}
      , notches_(shuffle_fwd_.size(), 0u)
      , len_{shuffle_fwd_.size()}
      , pos_{0} {
    assert(shuffle_fwd_.size() == shuffle_bck_.size());
    auto i{0u};
    // if shuffle_fwd_[i] is the output letter, then shuffle_bck_[out] == i 
    for (auto elem : shuffle_fwd_) {
      shuffle_bck_[elem - a_] = i++;
    }
    std::transform(shuffle_fwd_.begin(), shuffle_fwd_.end(),
                   shuffle_fwd_.begin(), [](uint32_t ch) { return ch - a_; });
    notches_[len_ - 1] = 1;
  }

// Ring setting rotates the inner wiring independently of the contacts - e.g.
// ring setting 0 
  Rotor(std::string_view shuffle, Shuffle notches,
        Shuffle::size_type initial_pos = 0, Shuffle::size_type ring_setting = 0)
      : shuffle_fwd_{shuffle.begin(), shuffle.end()}
      , shuffle_bck_(shuffle.size())
      , notches_{notches}
      , len_{shuffle_fwd_.size()}
      , pos_{initial_pos} {
    // Move this to a helper function that returns a movable thing with
    // the transform applied?
    std::transform(shuffle_fwd_.begin(), shuffle_fwd_.end(),
                   shuffle_fwd_.begin(), [](uint32_t ch) { return ch - a_; });
    auto i{0u};
    for (auto elem : shuffle_fwd_) {
      shuffle_bck_[elem] = i++;
    }
  }

  /* A Rotor can only turn when the previous Rotor also has turned, so return
   * whether it has turned for the next Rotor to use. Does not model the double
   * step "bug" from some models of Enigma Machine. */
  uint32_t rotate(uint32_t turnover) {
    pos_ = (pos_ - turnover) % len_;
    return notches_[pos_];
  }

  uint32_t connect_fwd(uint32_t character) {
    return shuffle_fwd_[(character + pos_) % len_];
  }

  uint32_t connect_bck(uint32_t character) {
    return shuffle_bck_[(character + pos_) % len_];
  }
};

class EnigmaMachine {
  // Vector of rotors, freely chosen.
  std::vector<Rotor> rotors_;
  // The plugboard is basically like a rotor that is symmetric and only remaps
  // up to ten pairs of characters (Enigma only came with ten wires).
  std::vector<uint32_t> plugboard_;
  // The ETW is the fixed rotor mapping from the keyboard to the rotors
  Rotor etw_;
  // The UKW is the fixed reflector at the end - must be symmetric
  Rotor ukw_;

public:
  EnigmaMachine()
      : rotors_{}
      , plugboard_(26u)
      , etw_(rotors::etw_alpha, make_notches(26u, {}))
      , ukw_(rotors::ukw_b, make_notches(26u, {})) {
    rotors_.emplace_back(rotors::i, make_notches(26u, {'Z' - 'A'}));
    rotors_.emplace_back(rotors::i, make_notches(26u, {'Z' - 'A'}));
    rotors_.emplace_back(rotors::i, make_notches(26u, {'Z' - 'A'}));
    std::iota(plugboard_.begin(), plugboard_.end(), 0u);
  }

  char cipher_one(char c) {
    auto chr = [](int C) { return static_cast<char>(C + 'A'); };
    uint32_t scratch = c - 'A';
    scratch = plugboard_[scratch];
    scratch = etw_.connect_fwd(scratch);
    for (auto &rotor : rotors_) {
      scratch = rotor.connect_fwd(scratch);
      std::cout << chr(scratch) << " ";
    }
    std::cout << std::endl;
    scratch = ukw_.connect_fwd(scratch);
    std::cout << chr(scratch) << "\n";
    for (auto &rotor : make_reverse(rotors_)) {
      scratch = rotor.connect_bck(scratch);
      std::cout << chr(scratch) << " ";
    }
    std::cout << std::endl;
    scratch = etw_.connect_bck(scratch);
    scratch = plugboard_[scratch];
    return scratch + 'A';
  }

  /* The first Rotor is always told to spin when the key is pressed, *before*
   * the character is ciphered. */
  void keydown() {
    auto rotated = 1u;
    for (auto &rotor : rotors_) {
      rotated = rotor.rotate(rotated);
    }
  }
};

int main() {
  EnigmaMachine machine;
  machine.keydown();
  char c;
  while (std::cin.good()) {
    std::cin >> c;
    machine.keydown();
    std::cout << machine.cipher_one(c) << "\n";
  }
}
