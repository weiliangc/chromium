'use strict';

function sensor_mocks(mojo) {
  return define('Generic Sensor API mocks', [
    'mojo/public/js/core',
    'mojo/public/js/bindings',
    'mojo/public/js/connection',
    'device/generic_sensor/public/interfaces/sensor_provider.mojom',
    'device/generic_sensor/public/interfaces/sensor.mojom',
  ], (core, bindings, connection, sensor_provider, sensor) => {

    // Helper function that returns resolved promise with result.
    function sensorResponse(success) {
      return Promise.resolve({success});
    }

    // Class that mocks Sensor interface defined in sensor.mojom
    class MockSensor {
      constructor(stub, handle, offset, size, reportingMode) {
        this.client_ = null;
        this.stub_ = stub;
        this.start_should_fail_ = false;
        this.reporting_mode_ = reportingMode;
        this.sensor_reading_timer_id_ = null;
        this.update_reading_function_ = null;
        this.suspend_called_ = null;
        this.resume_called_ = null;
        this.add_configuration_called_ = null;
        this.remove_configuration_called_ = null;
        this.active_sensor_configurations_ = [];
        let rv = core.mapBuffer(handle, offset, size,
            core.MAP_BUFFER_FLAG_NONE);
        assert_equals(rv.result, core.RESULT_OK, "Failed to map shared buffer");
        this.buffer_array_ = rv.buffer;
        this.buffer_ = new Float64Array(this.buffer_array_);
        this.resetBuffer();
        bindings.StubBindings(this.stub_).delegate = this;
        bindings.StubBindings(this.stub_).connectionErrorHandler = () => {
          reset();
        };
      }

      // Returns default configuration.
      getDefaultConfiguration() {
        return Promise.resolve({frequency: 5});
      }

      // Adds configuration for the sensor and starts reporting fake data
      // through update_reading_function_ callback.
      addConfiguration(configuration) {
        assert_not_equals(configuration, null, "Invalid sensor configuration.");

        if (!this.start_should_fail_ && this.update_reading_function_ != null) {
          let timeout = (1 / configuration.frequency) * 1000;
          this.sensor_reading_timer_id_ = window.setTimeout(() => {
            this.update_reading_function_(this.buffer_);
            if (this.reporting_mode_ === sensor.ReportingMode.ON_CHANGE) {
              this.client_.sensorReadingChanged();
            }
          }, timeout);
        }

        this.active_sensor_configurations_.push(configuration);

        if (this.add_configuration_called_ != null)
          this.add_configuration_called_(this);

        return sensorResponse(!this.start_should_fail_);
      }

      // Removes sensor configuration from the list of active configurations and
      // stops notification about sensor reading changes if
      // active_sensor_configurations_ is empty.
      removeConfiguration(configuration) {
        if (this.remove_configuration_called_ != null) {
          this.remove_configuration_called_(this);
        }

        let index = this.active_sensor_configurations_.indexOf(configuration);
        if (index !== -1) {
          this.active_sensor_configurations_.splice(index, 1);
        } else {
          return sensorResponse(false);
        }

        if (this.sensor_reading_timer_id_ != null
            && this.active_sensor_configurations_.length === 0) {
          window.clearTimeout(this.sensor_reading_timer_id_);
          this.sensor_reading_timer_id_ = null;
        }

        return sensorResponse(true);
      }

      // Suspends sensor.
      suspend() {
        if (this.suspend_called_ != null) {
          this.suspend_called_(this);
        }
      }

      // Resumes sensor.
      resume() {
        if (this.resume_called_ != null) {
          this.resume_called_(this);
        }
      }

      // Mock functions

      // Resets mock Sensor state.
      reset() {
        if (this.sensor_reading_timer_id_) {
          window.clearTimeout(this.sensor_reading_timer_id_);
          this.sensor_reading_timer_id_ = null;
        }

        this.start_should_fail_ = false;
        this.update_reading_function_ = null;
        this.active_sensor_configurations_ = [];
        this.suspend_called_ = null;
        this.resume_called_ = null;
        this.add_configuration_called_ = null;
        this.remove_configuration_called_ = null;
        this.resetBuffer();
        core.unmapBuffer(this.buffer_array_);
        this.buffer_array_ = null;
        bindings.StubBindings(this.stub_).close();
      }

      // Zeroes shared buffer.
      resetBuffer() {
        for (let i = 0; i < this.buffer_.length; ++i) {
          this.buffer_[i] = 0;
        }
      }

      // Sets callback that is used to deliver sensor reading updates.
      setUpdateSensorReadingFunction(update_reading_function) {
        this.update_reading_function_ = update_reading_function;
        return Promise.resolve(this);
      }

      // Sets flag that forces sensor to fail when addConfiguration is invoked.
      setStartShouldFail(should_fail) {
        this.start_should_fail_ = should_fail;
      }

      // Returns resolved promise if suspend() was called, rejected otherwise.
      suspendCalled() {
        return new Promise((resolve, reject) => {
          this.suspend_called_ = resolve;
        });
      }

      // Returns resolved promise if resume() was called, rejected otherwise.
      resumeCalled() {
        return new Promise((resolve, reject) => {
          this.resume_called_ = resolve;
        });
      }

      // Resolves promise when addConfiguration() is called.
      addConfigurationCalled() {
        return new Promise((resolve, reject) => {
          this.add_configuration_called_ = resolve;
        });
      }

      // Resolves promise when removeConfiguration() is called.
      removeConfigurationCalled() {
        return new Promise((resolve, reject) => {
          this.remove_configuration_called_ = resolve;
        });
      }

    }

    // Helper function that returns resolved promise for getSensor() function.
    function getSensorResponse(init_params, client_request) {
      return Promise.resolve({init_params, client_request});
    }

    // Class that mocks SensorProvider interface defined in
    // sensor_provider.mojom
    class MockSensorProvider {
      constructor() {
        this.reading_size_in_bytes_ =
            sensor_provider.SensorInitParams.kReadBufferSizeForTests;
        this.shared_buffer_size_in_bytes_ = this.reading_size_in_bytes_ *
                sensor.SensorType.LAST;
        let rv =
                core.createSharedBuffer(
                        this.shared_buffer_size_in_bytes_,
                        core.CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE);
        assert_equals(rv.result, core.RESULT_OK, "Failed to create buffer");
        this.shared_buffer_handle_ = rv.handle;
        this.active_sensor_ = null;
        this.get_sensor_should_fail_ = false;
        this.resolve_func_ = null;
        this.is_continuous_ = false;
      }

      // Returns initialized Sensor proxy to the client.
      getSensor(type, stub) {
        if (this.get_sensor_should_fail_) {
          return getSensorResponse(null,
              connection.bindProxy(null, sensor.SensorClient));
        }

        let offset =
                (sensor.SensorType.LAST - type) * this.reading_size_in_bytes_;
        let reporting_mode = sensor.ReportingMode.ON_CHANGE;
        if (this.is_continuous_) {
            reporting_mode = sensor.ReportingMode.CONTINUOUS;
        }

        if (this.active_sensor_ == null) {
          let mockSensor = new MockSensor(stub, this.shared_buffer_handle_,
              offset, this.reading_size_in_bytes_, reporting_mode);
          this.active_sensor_ = mockSensor;
        }

        let rv =
                core.duplicateBufferHandle(
                        this.shared_buffer_handle_,
                        core.DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE);

        assert_equals(rv.result, core.RESULT_OK);

        let default_config = {frequency: 5};

        let init_params =
            new sensor_provider.SensorInitParams(
                { memory: rv.handle,
                  buffer_offset: offset,
                  mode: reporting_mode,
                  default_configuration: default_config });

        if (this.resolve_func_ !== null) {
          this.resolve_func_(this.active_sensor_);
        }

        var client_handle = connection.bindProxy(proxy => {
          this.active_sensor_.client_ = proxy;
          }, sensor.SensorClient);

        return getSensorResponse(init_params, client_handle);
      }

      // Binds object to mojo message pipe
      bindToPipe(pipe) {
        this.stub_ = connection.bindHandleToStub(
            pipe, sensor_provider.SensorProvider);
        bindings.StubBindings(this.stub_).delegate = this;
      }

      // Mock functions

      // Resets state of mock SensorProvider between test runs.
      reset() {
        if (this.active_sensor_ != null) {
          this.active_sensor_.reset();
          this.active_sensor_ = null;
        }

        this.get_sensor_should_fail_ = false;
        this.resolve_func_ = null;
      }

      // Sets flag that forces mock SensorProvider to fail when getSensor() is
      // invoked.
      setGetSensorShouldFail(should_fail) {
        this.get_sensor_should_fail_ = should_fail;
      }

      // Returns mock sensor that was created in getSensor to the layout test.
      getCreatedSensor() {
        if (this.active_sensor_ != null) {
          return Promise.resolve(this.active_sensor_);
        }

        return new Promise((resolve, reject) => {
          this.resolve_func_ = resolve;
         });
      }

      // Forces sensor to use |reporting_mode| as an update mode.
      setContinuousReportingMode(reporting_mode) {
          this.is_continuous_ = reporting_mode;
      }
    }

    let mockSensorProvider = new MockSensorProvider;
    mojo.frameInterfaces.addInterfaceOverrideForTesting(
        sensor_provider.SensorProvider.name,
        pipe => {
          mockSensorProvider.bindToPipe(pipe);
        });

    return Promise.resolve({
      mockSensorProvider: mockSensorProvider,
    });
  });
}

function sensor_test(func, name, properties) {
  mojo_test(mojo => sensor_mocks(mojo).then(sensor => {
    // Clean up and reset mock sensor stubs asynchronously, so that the blink
    // side closes its proxies and notifies JS sensor objects before new test is
    // started.
    let onSuccess = () => {
      sensor.mockSensorProvider.reset();
      return new Promise((resolve, reject) => { setTimeout(resolve, 0); });
    };

    let onFailure = () => {
      sensor.mockSensorProvider.reset();
      return new Promise((resolve, reject) => { setTimeout(reject, 0); });
    };

    return Promise.resolve(func(sensor)).then(onSuccess, onFailure);
  }), name, properties);
}
