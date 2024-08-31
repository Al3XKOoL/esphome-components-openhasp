#include "graphic_display.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphics {

static const char *const TAG = "GraphicsDisplay";

void Rect::expand(int16_t width, int16_t height) {
  if ((*this).is_set() && ((*this).width >= (-2 * width)) &&
      ((*this).height >= (-2 * height))) {
    (*this).x = (*this).x - width;
    (*this).y = (*this).y - height;
    (*this).width = (*this).width + (2 * width);
    (*this).height = (*this).height + (2 * height);
  }
}

void Rect::join(Rect rect) {
  if (!this->is_set()) {
    this->x = rect.x;
    this->y = rect.y;
    this->width = rect.width;
    this->height = rect.height;
  } else {
    if (this->x > rect.x) {
      this->x = rect.x;
    }
    if (this->y > rect.y) {
      this->y = rect.y;
    }
    if (this->x2() < rect.x2()) {
      this->width = rect.x2() - this->x;
    }
    if (this->y2() < rect.y2()) {
      this->height = rect.y2() - this->y;
    }
  }
}
void Rect::substract(Rect rect) {
  if (!this->inside(rect)) {
    (*this) = Rect();
  } else {
    if (this->x < rect.x) {
      this->x = rect.x;
    }
    if (this->y < rect.y) {
      this->y = rect.y;
    }
    if (this->x2() > rect.x2()) {
      this->width = rect.x2() - this->x;
    }
    if (this->y2() > rect.y2()) {
      this->height = rect.y2() - this->y;
    }
  }
}
bool Rect::inside(int16_t x, int16_t y, bool absolute) {
  if (!this->is_set()) {
    return true;
  }
  if (absolute) {
    return ((x >= 0) && (x <= this->width) && (y >= 0) && (y <= this->height));
  } else {
    return ((x >= this->x) && (x <= this->x2()) && (y >= this->y) &&
            (y <= this->y2()));
  }
}
bool Rect::inside(Rect rect, bool absolute) {
  if (!this->is_set() || !rect.is_set()) {
    return true;
  }
  if (absolute) {
    return ((rect.x <= this->width) && (rect.width >= 0) &&
            (rect.y <= this->height) && (rect.height >= 0));
  } else {
    return ((rect.x <= this->x2()) && (rect.x2() >= this->x) &&
            (rect.y <= this->y2()) && (rect.y2() >= this->y));
  }
}
void Rect::info(const std::string &prefix) {
  if (this->is_set()) {
    ESP_LOGI(TAG, "%s [%3d,%3d,%3d,%3d]", prefix.c_str(), this->x, this->y,
             this->width, this->height);
  } else
    ESP_LOGI(TAG, "%s ** IS NOT SET **", prefix.c_str());
}

void Clipping::push_clipping(Rect rect) {
  // ESP_LOGW(TAG, "set: Push new clipping");
  if (!this->clipping_rectangle_.empty()) {
    Rect r = this->clipping_rectangle_.back();
    rect.substract(r);
  }
  this->clipping_rectangle_.push_back(rect);
}
void Clipping::pop_clipping() {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "clear: Clipping is not set.");
  } else {
    this->clipping_rectangle_.pop_back();
  }
}
void Clipping::add_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    this->clipping_rectangle_.back().join(add_rect);
  }
}
void Clipping::substract_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    // ESP_LOGW(TAG, "add: join new clipping");
    this->clipping_rectangle_.back().substract(add_rect);
  }
}
Rect Clipping::get_clipping() {
  if (this->clipping_rectangle_.empty()) {
    return Rect();
  } else {
    return this->clipping_rectangle_.back();
  }
}

void GraphicsDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "Display Graphics");
  ESP_LOGCONFIG(TAG, "  Rotations: %d °", (this)->get_rotation());
  ESP_LOGCONFIG(TAG, "  Dimensions: %dpx x %dpx", (this)->get_width(),
                (this)->get_height());
  LOG_UPDATE_INTERVAL(this);
}

void GraphicsDisplay::setup() {
  // Configurar pines
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_WR, OUTPUT);
  pinMode(TFT_RD, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  for (int i = 0; i < 8; i++) {
    pinMode(TFT_D0 + i, OUTPUT);
  }

  // Reset del display
  digitalWrite(TFT_RST, HIGH);
  delay(5);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  // Inicialización del display
  write_command(0xEF);
  write_data(0x03);
  write_data(0x80);
  write_data(0x02);
  
  // ... (añade aquí el resto de los comandos de inicialización del ILI9341)

  this->clear();
  this->has_started_ = true;
}

void GraphicsDisplay::update() {
  static bool prossing_update = false, need_update = false;
  if (prossing_update) {
    need_update = true;
    return;
  }
  if (this->auto_clear_enabled_) {
    this->clear();
  }
  do {
    prossing_update = true;
    need_update = false;
    if (!need_update && this->writer_.has_value()) {
      (*this->writer_)(*this);
    }
  } while (need_update);
  flush();
  prossing_update = false;
}

void GraphicsDisplay::write_command(uint8_t cmd) {
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  write_8bit(cmd);
  digitalWrite(TFT_CS, HIGH);
}

void GraphicsDisplay::write_data(uint8_t data) {
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  write_8bit(data);
  digitalWrite(TFT_CS, HIGH);
}

void GraphicsDisplay::write_8bit(uint8_t data) {
  digitalWrite(TFT_WR, LOW);
  WRITE_PERI_REG(GPIO_OUT_REG, (READ_PERI_REG(GPIO_OUT_REG) & ~0xFF) | data);
  digitalWrite(TFT_WR, HIGH);
}

void GraphicsDisplay::set_address_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  write_command(ILI_CASET);
  write_data(x1 >> 8);
  write_data(x1 & 0xFF);
  write_data(x2 >> 8);
  write_data(x2 & 0xFF);

  write_command(ILI_PASET);
  write_data(y1 >> 8);
  write_data(y1 & 0xFF);
  write_data(y2 >> 8);
  write_data(y2 & 0xFF);

  write_command(ILI_RAMWR);
}

void GraphicsDisplay::draw_pixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= this->get_width() || y < 0 || y >= this->get_height())
    return;
  
  set_address_window(x, y, x, y);
  write_data(color >> 8);
  write_data(color & 0xFF);
}

void GraphicsDisplay::fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x >= this->get_width() || y >= this->get_height())
    return;
  
  int16_t x2 = x + w - 1, y2 = y + h - 1;
  if (x2 < 0 || y2 < 0)
    return;
  
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x2 >= this->get_width()) x2 = this->get_width() - 1;
  if (y2 >= this->get_height()) y2 = this->get_height() - 1;
  
  set_address_window(x, y, x2, y2);
  
  uint8_t hi = color >> 8, lo = color & 0xFF;
  for (int32_t i = (int32_t)(y2 - y + 1) * (x2 - x + 1); i > 0; i--) {
    write_data(hi);
    write_data(lo);
  }
}

} // namespace graphics
} // namespace esphome
