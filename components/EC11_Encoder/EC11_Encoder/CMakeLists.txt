# EC11 Rotary Encoder Driver for ESP-IDF
# Quadrature decoder with GPIO interrupt handling

cmake_minimum_required(VERSION 3.5)

idf_component_register(
    SRCS "src/ec11_encoder.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES driver esp_driver_gpio freertos esp_timer
)

